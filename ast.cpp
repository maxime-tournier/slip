#include "ast.hpp"

#include <map>
#include <set>

#include <functional>

#include "maybe.hpp"

#include "sexpr.hpp"
#include "tool.hpp"

#include "slice.hpp"
#include "repr.hpp"

namespace ast {

  static var check_var(symbol s);
    
  struct unimplemented : std::runtime_error {
    unimplemented(std::string what)
      : std::runtime_error("unimplemented: " + what) { }
  };
  
  symbol abs::arg::name() const {
    return match([](var self) { return self.name; },
                 [](typed self) { return self.id.name; });
  }



  def::def(var id, const expr& value) : id(id), value(make_expr(value)) { }
  
  static const symbol arrow = "->";
  
  static sexpr::list rewrite_arrows(sexpr::list args) {
    if(!args) return args;

    // (x -> ...) ~> (-> x ...)
    if(args->tail && args->tail->head == arrow) {
      auto tail = rewrite_arrows(args->tail->tail);
      if(tail->tail) {
        return arrow >>= args->head >>= tail >>= sexpr::list();        
      } else {
        return arrow >>= args->head >>= tail;
      }
    }
    
    return args->head >>= rewrite_arrows(args->tail);
  }

  
  template<class U>
  using monad = slice::monad<sexpr, U>;

  
  template<class U>
  using slice = slice::slice<sexpr, U>;

  
  using namespace slice;

  
  // pop if head is of given type
  template<class U>
  static slice<U> pop_as(const list<sexpr>& self) {
    if(!self || !self->head.template get<U>()) {
      return {{}, self};
    }
    
    return {self->head.cast<U>(), self->tail};
  }

  
  // throw on error
  template<class U, class Exc>
  static monad<U> raise(Exc exc) {
    return [exc](sexpr::list) -> slice<U> {
      throw exc;
    };
  }

  
  // check application
  static slice<expr> check_app(sexpr::list args) {
    if(!args) throw error("empty list in application");
      
    // TODO don't rewrite **all the time
    sexpr::list rw = rewrite_arrows(args);
      
    const auto func = expr::check(rw->head);
    const expr res = app{func, map(rw->tail, expr::check)};
      
    return {res, nullptr};
  }

  
  // validate sexpr against a special forms table
  template<class U>
  using special_type = std::map<symbol, std::pair<monad<U>, std::string> >;
      
  template<class U>
  static monad<U> check_special(const special_type<U>& table) {
    return pop_as<symbol> >> [&](symbol s) -> monad<U> {
      const auto it = table.find(s);
      if(it != table.end()) {
        const error err(it->second.second);
        return it->second.first | raise<U>(err);
      }
      return fail<U>();
    };
  }

  // check typed function argument
  static const auto check_typed_arg = pop() >> [](sexpr type) {
    return pop_as<symbol> >> [type](symbol name) {
      return done(abs::typed{expr::check(type), check_var(name)});
    };
  };


  static const auto check_arg = [](sexpr self) -> maybe<abs::arg> {
    return self.match([&](symbol self) {
        return just(abs::arg(check_var(self)));
      },
      [&](sexpr::list self) {
        return check_typed_arg(self) >> [](abs::typed self) {
          return just(abs::arg(self));
        };
      },
      [&](sexpr) -> maybe<abs::arg> {
        return {};
      });
  };
  

  // check function arguments
  static const auto check_args = map(check_arg);
  

  // check fundefs
  static const auto check_abs = pop_as<sexpr::list> >> [](sexpr::list self) {
    return lift(check_args(self)) >> [](abs::arg::list args) {
      return pop() >> [&](const sexpr& body) -> monad<expr> {
        const expr e = abs{args, expr::check(body)};
        return done(e);
      };
    };
  };

  // check a binding (`symbol`, `expr`)
  static const auto check_bind = pop_as<symbol> >> [](symbol id) {
    return pop() >> [id](sexpr e) {
      return done(bind{check_var(id), expr::check(e)});
    };
  };


  // check a binding sequence
  static const auto check_bindings = map([](sexpr self) -> maybe<bind> {
      if(auto binding = self.get<sexpr::list>()) {
        return check_bind(*binding);
      }
      return {};
    });

  // check let-bindings
  static const auto check_let = pop_as<sexpr::list> >> [](sexpr::list bindings) -> monad<expr> {
    return lift(check_bindings(bindings)) >> [](list<bind> defs) {
      return pop() >> [defs](sexpr body) {
        const expr res = let{defs, expr::check(body)};
        return done(res);
      };
    };
  };


  struct named_signature {
    const var id;
    const abs::arg::list args;
  };

  // check a named signature `symbol` `arg`...
  static const auto check_named_signature = pop_as<symbol> >> [](symbol name) {
    return check_args >> [=](abs::arg::list args) {
      return pure(named_signature{check_var(name), args});
    };
  };

  // check inline function definition (def (foo a b c) bar)
  static const auto check_fundef = pop_as<sexpr::list> >> [](sexpr::list sig) {
    return lift(check_named_signature(sig)) >> [](named_signature sig) {
      return pop() >> [=](sexpr body) {
        const ast::bind res{sig.id, ast::abs{sig.args, expr::check(body)}};
        return done(res);
      };
    };
  };

  // check definitions
  static const auto check_def = (check_bind | check_fundef) >> [](bind self) {
    const expr res = def{self.id, self.value};
    return pure(res);
  };
  

  // check sequences
  static slice<expr> check_seq(sexpr::list args) {
    const expr res = seq{map(args, io::check)};
    return {res, nullptr};
  }


  // check conditionals
  static const auto check_cond = pop() >> [](sexpr self) {
    const auto test = expr::check(self);
    return pop() >> [=](sexpr self) {
      const auto conseq = expr::check(self);
      return pop() >> [=](sexpr self) {
        const auto alt = expr::check(self);
        const expr res = cond{test, conseq, alt};
        return pure(res);
      };
    };
  };


  // check record attributes
  static const auto check_record_attrs = map([](sexpr e) -> maybe<record::attr> {
      if(auto self = e.get<sexpr::list>()) {
        return check_bind(*self) >> [](bind b) {
          const record::attr head{b.id, b.value};
          return just(head);
        };
      }
      return {};
    });

  // check record literals
  static const auto check_record = check_record_attrs >> [](record::attr::list attrs) {
    const expr res = record{attrs};
    return done(res);
  };


  // check module creation
  static const auto check_make = pop() >> [](sexpr type) {
    
    return check_record >> [type](expr self) {
      const expr res = make{ make_expr(expr::check(type)),
                             self.cast<record>().attrs};
      return done(res);
    };
  };

  // check module unpacking
  static const auto check_use = pop() >> [](sexpr env) {
    return pop() >> [env](sexpr body) {
      const expr res = use(expr::check(env), expr::check(body));
      return done(res);
    };
  };

  // check package import
  static const auto check_import = pop_as<symbol> >> [](symbol package) {
    const expr res = import{package};
    return done(res);
  };


  // check module declaration without arguments
  static const auto module_noargs = pop_as<symbol> >> [](symbol name) {
    return pure(named_signature{check_var(name), nullptr});
  };

  // check module declaration with arguments  
  static const auto module_args = pop_as<sexpr::list> >> [](sexpr::list sig) {
    return lift(check_named_signature(sig));
  };
  

  // check module definition
  static const auto check_module = (module_noargs | module_args) >> [](named_signature self) {
    return check_record_attrs >> [=](record::attr::list attrs) {
      const expr res = module{self.id, self.args, attrs};
      return done(res);
    };
  };


  // check a match handler
  static const auto check_match_handler = pop_as<symbol> >> [](symbol name) {
    return pop() >> [=](sexpr arg) {
      return lift(check_arg(arg)) >> [=](abs::arg arg) {
        return pop() >> [=](sexpr body) {
          const match::handler res = {check_var(name), arg, expr::check(body)};
          return done(res);
        };
      };
    };
  };

  static const auto check_match_with_value = pop() >> [](sexpr value) {
    return map([](sexpr item) -> maybe<match::handler> {
        if(auto self = item.get<sexpr::list>()) {
          return check_match_handler(*self);
        }
        return {};
      }) >> [=](match::handler::list cases) {
      const expr res = app{match{cases}, expr::check(value) >>= expr::list()};
      return pure(res);
    };
  };

  
  // check pattern matching TODO match without value
  static const auto check_match = check_match_with_value;

  
  namespace kw {
    // reserved keywords
    symbol abs("fn"),
      let("let"),
      seq("do"),
      def("def"),
      cond("if"),
      record("record"),
      match("match"),
      make("new"),
      bind("bind"),
      use("use"),
      import("import"),
      module("module")
      ;
    
    static const std::set<symbol> reserved = {
      abs, let, def, cond,
      record,
      match,
      bind, seq,
      make, use, import,
      module,
    };

  }
  
  // special forms table
  static const special_type<expr> special_expr = {
    {kw::abs, {check_abs, "(fn (`arg`...) `expr`)"}},
    {kw::let, {check_let, "(let ((`symbol` `expr`)...) `expr`)"}},    
    {kw::seq, {check_seq, "(do ((bind `symbol` `expr`) | `expr`)...)"}},
    {kw::cond, {check_cond, "(if `expr` `expr` `expr`)"}},
    {kw::record, {check_record, "(record (`symbol` `expr`)...)"}},
    {kw::match, {check_match, "(match `expr` (`symbol` `arg` `expr`)...)"}},
    {kw::make, {check_make, "(new `symbol` (`symbol` `expr`)...)"}},
    {kw::def, {check_def, "(def `symbol` `expr`)"}},
    {kw::use, {check_use, "(use `expr` `expr`)"}},
    {kw::import, {check_import, "(import `symbol`)"}},
    {kw::module, {check_module,
                  "(module (`symbol` `arg`...) (`symbol `expr`)...)"}},
  };

  static const special_type<io> special_io = {
    {kw::bind, {check_bind >> [](bind self) {
      return pure(io(self));
    }, "(bind `symbol` `expr`)"}},
  };

  static var check_identifier(symbol s) {
    // forbid reserved keywords
    if(kw::reserved.find(s) != kw::reserved.end()) {
      throw error(tool::quote(s.get()) + " is a reserved keyword and"
                  " cannot be used as a variable name");
    }
    
    return {s};
  }
  
  static expr check_symbol(symbol s) {

    // selection
    if(s.get()[0] == selection_prefix) {
      const std::string name = std::string(s.get()).substr(1);
      if(name.empty()) throw error("empty attribute name");
      return sel{check_var(name)};
    }


    // injection
    if(s.get()[0] == injection_prefix) {
      const std::string name = std::string(s.get()).substr(1);
      if(name.empty()) throw error("empty case name");
      return inj{check_var(name)};
    }
    
    
    // special cases
    if(s == "true") { return lit<boolean>{true}; }
    if(s == "false") { return lit<boolean>{false}; }

    return check_identifier(s);
  }
  

  static var check_var(symbol s) {
    const expr e = check_symbol(s);
    if(auto self = e.get<var>()) {
      return *self;
    }

    throw error(tool::quote(s.get()) + " cannot be used as a variable name");
  }
  
  
  expr expr::check(const sexpr& e) {
    static const auto impl = check_special(special_expr) | check_app;
    
    return e.match([](boolean b) -> expr { return make_lit(b); },
                   [](integer i) -> expr { return make_lit(i); },
                   [](real r) -> expr { return make_lit(r); },
                   [](symbol s) -> expr {
                     return check_symbol(s);
                   },
                   [](sexpr::list f) -> expr {
                     return impl(f).get();
                   });
  }
  

  io io::check(const sexpr& e) {
    static const auto impl = check_special(special_io);

    return e.match([](sexpr::list self) -> io {
        if(auto res = impl(self)) {
          return res.get();
        }
        return expr::check(self);
      },[](sexpr self) -> io { return expr::check(self); });
  }


  std::ostream& operator<<(std::ostream& out, const expr& self) {
    return out << repr(self);
  }


  void expr::iter(std::istream& in, std::function<void(expr)> cont) {
    sexpr::iter(in, [cont](sexpr e) {
      cont(expr::check(e));
    });
  }
  
}

