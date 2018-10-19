#include "ast.hpp"

#include <map>
#include <set>

#include <functional>

#include "maybe.hpp"

#include "sexpr.hpp"
#include "tool.hpp"

#include "slice.hpp"


namespace ast {

  static var check_var(symbol s);
    
  struct unimplemented : std::runtime_error {
    unimplemented(std::string what)
      : std::runtime_error("unimplemented: " + what) { }
  };
  
  symbol abs::arg::name() const {
    return match<symbol>
      ([](var self) { return self.name; },
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
    

  static const auto check_args = peek() >> [](sexpr::list self) {
    maybe<abs::arg::list> init{nullptr};
    
    return lift(foldr(init, self, [](sexpr lhs, maybe<abs::arg::list> rhs) {
          return rhs >> [&](abs::arg::list tail) -> maybe<abs::arg::list> {
            if(auto res = lhs.get<symbol>()) {
              return check_var(*res) >>= tail;
            }
            if(auto res = lhs.get<sexpr::list>()) {
              return (pop() >> [tail](sexpr type) {
                  return pop_as<symbol> >> [tail, type](symbol name) {
                    return pure(abs::typed{expr::check(type), check_var(name)} >>= tail);
                  };
                })(*res);
            }
            return {};
          };
        }));
  };
  


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


  static const auto check_bindings = map([](sexpr self) -> maybe<bind> {
      if(auto binding = self.get<sexpr::list>()) {
        return check_bind(*binding);
      }
      return {};
    });
  
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
  
  static const auto check_named_signature = pop_as<symbol> >> [](symbol name) {
    return check_args >> [=](abs::arg::list args) {
      return pure(named_signature{check_var(name), args});
    };
  };
  
  static const auto check_fundef = pop_as<sexpr::list> >> [](sexpr::list sig) {
    return lift(check_named_signature(sig)) >> [](named_signature sig) {
      return pop() >> [=](sexpr body) {
        const ast::bind res{sig.id, ast::abs{sig.args, expr::check(body)}};
        return done(res);
      };
    };
  };
  
  static const auto check_def = (check_bind | check_fundef) >> [](bind self) {
    const expr res = def{self.id, self.value};
    return pure(res);
  };
  

  static slice<expr> check_seq(sexpr::list args) {
    const expr res = seq{map(args, io::check)};
    return {res, nullptr};
  }

  
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


  static slice<record::attr::list> check_record_attrs(sexpr::list args) {
    const auto init = just(record::attr::list());
    
    // build attribute list by folding args
    if(auto res = foldr(init, args, [](sexpr e, maybe<record::attr::list> tail) {
          return tail >> [&](record::attr::list tail) {
            if(auto self = e.get<sexpr::list>()) {
              return check_bind(*self) >> [&](bind b) {
                const record::attr head{b.id, b.value};
                return just(head >>= tail);
              };
            } else {
              return maybe<record::attr::list>();
            }
          };
        })) {
      return {res.get(), nullptr};
    } else {
      return {{}, args};
    }
  };

  
  static const auto check_record = check_record_attrs >> [](record::attr::list attrs) {
    const expr res = record{attrs};
    return done(res);
  };
  
  static const auto check_make = pop() >> [](sexpr type) {
    
    return check_record >> [type](expr self) {
      const expr res = make{ make_expr(expr::check(type)),
                             self.cast<record>().attrs};
      return done(res);
    };
  };

  
  static const auto check_use = pop() >> [](sexpr env) {
    return pop() >> [env](sexpr body) {
      const expr res = use(expr::check(env), expr::check(body));
      return done(res);
    };
  };

  
  static const auto check_import = pop_as<symbol> >> [](symbol package) {
    const expr res = import{package};
    return done(res);
  };


  static const auto module_noargs = pop_as<symbol> >> [](symbol name) {
    return pure(named_signature{check_var(name), nullptr});
  };

  static const auto module_args = pop_as<sexpr::list> >> [](sexpr::list sig) {
    return lift(check_named_signature(sig));
  };
  

  static const auto check_module = (module_noargs | module_args) >> [](named_signature self) {
    return check_record_attrs >> [=](record::attr::list attrs) {
      const expr res = module{self.id, self.args, attrs};
      return done(res);
    };
    
  }; 
  
  
  namespace kw {

    symbol abs("fn"),
      let("let"),
      seq("do"),
      def("def"),
      cond("if"),
      record("record"),
      make("new"),
      bind("bind"),
      use("use"),
      import("import"),
      module("module")
      ;
   

    static const std::set<symbol> reserved = {
      abs, let, def, cond, record, bind, seq, make, use, import, module,
    };

  }
  
  // special forms table
  static const special_type<expr> special_expr = {
    {kw::abs, {check_abs, "(fn (`symbol`...) `expr`)"}},
    {kw::let, {check_let, "(let ((`symbol` `expr`)...) `expr`)"}},    
    {kw::seq, {check_seq, "(do ((bind `symbol` `expr`) | `expr`)...)"}},
    {kw::cond, {check_cond, "(if `expr` `expr` `expr`)"}},
    {kw::record, {check_record, "(record (`symbol` `expr`)...)"}},
    {kw::make, {check_make, "(new `symbol` (`symbol` `expr`)...)"}},
    {kw::def, {check_def, "(def `symbol` `expr`)"}},
    {kw::use, {check_use, "(use `expr` `expr`)"}},
    {kw::import, {check_import, "(import `symbol`)"}},
    {kw::module, {check_module,
                  "(module (`symbol` `symbol`...) (`symbol `expr`)...)"}},
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
    // attributes
    if(s.get()[0] == '.') {
      const std::string name = std::string(s.get()).substr(1);
      if(name.empty()) throw error("empty attribute name");
      return sel{name};
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
    
    return e.match<expr>
      ([](boolean b) { return make_lit(b); },
       [](integer i) { return make_lit(i); },
       [](real r) { return make_lit(r); },
       [](symbol s) -> expr {
         return check_symbol(s);
       },
       [](sexpr::list f) {
         return impl(f).get();
       });
  }
  

  io io::check(const sexpr& e) {
    static const auto impl = check_special(special_io);

    return e.match<io>
      ([](sexpr::list self) -> io {
        if(auto res = impl(self)) {
          return res.get();
        }
        return expr::check(self);
      },[](sexpr self) -> io { return expr::check(self); });
  }

  
  namespace {
    struct repr {
      using type = sexpr;
      
      template<class T>
      sexpr operator()(const lit<T>& self) const {
        return symbol("lit") >>= self.value >>= list<sexpr>();
      }

      sexpr operator()(const lit<unit>& self) const {
        return list<sexpr>();
      }

      sexpr operator()(const var& self) const {
        return symbol("var") >>= self.name >>= list<sexpr>();
      }

      sexpr operator()(const symbol& self) const {
        return self;
      }

      sexpr operator()(const abs::typed& self) const {
        return self.type.visit(*this) >>= self.id.name >>= sexpr::list();
      }
      

      sexpr operator()(const abs& self) const {
        return kw::abs
          >>= map(self.args, [](abs::arg arg) -> sexpr {
              return arg.visit(repr());
            })
          >>= self.body->visit(repr())
          >>= list<sexpr>();
      }


      sexpr operator()(const app& self) const {
        return symbol("app")
          >>= self.func->visit(repr())
          >>= map(self.args, [](const expr& e) {
              return e.visit(repr());
            })
          >>= list<sexpr>();
      }


      sexpr operator()(const let& self) const {
        return kw::let
          >>= map(self.defs, [](const bind& self) -> sexpr {
            return self.id.name >>= self.value.visit(repr()) >>= sexpr::list();
          })
          >>= self.body->visit(repr())
          >>= list<sexpr>();
      }


      sexpr operator()(const seq& self) const {
        return kw::seq
          >>= map(self.items, repr())
          >>= list<sexpr>();
      }


      sexpr operator()(const def& self) const {
        return kw::def
          >>= self.id.name
          >>= self.value->visit(repr())
          >>= list<sexpr>();
      }

    
      sexpr operator()(const io& self) const {
        return self.visit(repr());
      }


      sexpr operator()(const expr& self) const {
        return self.visit(repr());
      }


      sexpr operator()(const sel& self) const {
        return symbol("sel")
          >>= self.id.name
          >>= sexpr::list();        
      }


      sexpr operator()(const record& self) const {
        return kw::record
          >>= map(self.attrs, [](record::attr attr) -> sexpr {
            return attr.id.name >>= attr.value.visit(repr()) >>= sexpr::list();
          });
      }


      sexpr operator()(const cond& self) const {
        return kw::cond
          >>= self.test->visit(repr())
          >>= self.conseq->visit(repr())
          >>= self.alt->visit(repr())
          >>= sexpr::list();
      }


      sexpr operator()(const make& self) const {
        return kw::make
          >>= self.type->visit(repr())
          >>= map(self.attrs, [](record::attr attr) -> sexpr {
            return attr.id.name >>= attr.value.visit(repr()) >>= sexpr::list();
          });
      }

      sexpr operator()(const use& self) const {
        return kw::use
          >>= self.env->visit(repr())
          >>= self.body->visit(repr())
          >>= sexpr::list();
      }

      sexpr operator()(const import& self) const {
        return kw::import
          >>= self.package
          >>= sexpr::list();
      }
      
      template<class T>
      sexpr operator()(const T& self) const {
        throw std::logic_error("unimplemented repr: " + tool::type_name(typeid(T)));
      }
    
    };
  }

  std::ostream& operator<<(std::ostream& out, const expr& self) {
    return out << self.visit(repr());
  }


  std::ostream& operator<<(std::ostream& out, const io& self) {
    return out << self.visit(repr());
  }
  

  void expr::iter(std::istream& in, std::function<void(expr)> cont) {
    sexpr::iter(in, [cont](sexpr e) {
      cont(expr::check(e));
    });
  }
  
}

