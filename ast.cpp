#include "ast.hpp"

#include <map>
#include <set>

#include <functional>

#include "maybe.hpp"

#include "sexpr.hpp"
#include "tool.hpp"
#include "unpack.hpp"

namespace ast {
  
  template<class U>
  using monad = unpack::monad<sexpr, U>;

  using namespace unpack;
  
  // validate sexpr against a special forms table
  template<class U>
  using special_type = std::map<symbol, std::pair<monad<U>, std::string> >;
      
  template<class U>
  static monad<U> check_special(const special_type<U>& table) {
    return pop_as<symbol>() >> [&](symbol s) -> monad<U> {
      const auto it = table.find(s);
      if(it != table.end()) {
        const error err(it->second.second);
        return it->second.first | unpack::error<U>(err);
      }
      return fail<U>();
    };
  }
    
  struct unimplemented : std::runtime_error {
    unimplemented(std::string what)
      : std::runtime_error("unimplemented: " + what) { }
  };
    
  
  // check function calls
  static maybe<expr> check_call(sexpr::list args) {
    if(!args) throw error("empty list in application");
    
    const auto func = expr::check(args->head);
    const expr res = app{func, map(args->tail, expr::check)};
    return res;
  }

  symbol abs::arg::name() const {
    return match<symbol>
      ([](symbol self) { return self; },
       [](typed self) { return self.name; });
  }
  
// struct typed_argument {
//   const symbol type;
//   const symbol name;
// };

// struct argument : variant<symbol, typed_argument> {
//   using argument::variant::variant;
// };

  using args_type = list<abs::arg>;

static maybe<args_type> check_args(sexpr::list self) {
  maybe<args_type> init(nullptr);
  return foldr(init, self, [](sexpr lhs, maybe<args_type> rhs) {
    return rhs >> [&](args_type tail) -> maybe<args_type> {
          
      if(auto res = lhs.get<symbol>()) {
        return *res >>= tail;
      }
      if(auto res = lhs.get<sexpr::list>()) {
        return (pop_as<symbol>() >> [tail](symbol type){
          return pop_as<symbol>() >> [tail, type](symbol name) {
            return pure(abs::typed{type, name} >>= tail);
          };
          })(*res);
      }
      return {};
    };
  });
}


// 
  static const auto check_abs = pop_as<sexpr::list>() 
    >> [](sexpr::list self) {
    return pop() >> [&](const sexpr& body) -> monad<expr> {
      if(const auto args = check_args(self)) {
        const expr e = abs{args.get(), make_ref<expr>(expr::check(body))};
        return done(pure(e));
      } else {
        return fail<expr>();
      }
    };
  };

// check a binding (`symbol`, `expr`)
static const auto check_binding =
  pop_as<symbol>() >> [](symbol id) {
    return pop() >> [id](sexpr e) {
      return done(pure(def{id, expr::check(e)}));
    };
  };

// check a binding sequence (`binding`...)
static const auto check_bindings = [](sexpr::list self) {
  list<def> defs;
  return foldr(just(defs), self, [](sexpr lhs, maybe<list<def>> rhs) {
      return rhs >> [&](list<def> tail) -> maybe<list<def>> {
        if(auto binding = lhs.get<sexpr::list>()) {
          return check_binding(*binding) >> [&](def self) {
            return just(self >>= tail);
          };
        }
        return {};
    };
  });
};
  

static const auto check_let = pop_as<sexpr::list>()
  >> [](sexpr::list bindings) -> monad<expr> {
    if(const auto defs = check_bindings(bindings)) {
      return pop() >> [defs](sexpr body) {
        const expr res = let{defs.get(), make_ref<expr>(expr::check(body))};
        return done(pure(res));
      };
    } else return fail<expr>();
};


static const auto check_def = check_binding >> [](def self) {
  const io res = self;
  return pure(res);
};


static maybe<expr> check_seq(sexpr::list args) {
  const expr res = seq{map(args, io::check)};
  return res;
}

static const auto check_cond =
  pop()
  >> [](sexpr self) {
    auto test = make_ref<expr>(expr::check(self));
    return pop()
      >> [=](sexpr self) {
        auto conseq = make_ref<expr>(expr::check(self));
        return pop()
          >> [=](sexpr self) {
            auto alt = make_ref<expr>(expr::check(self));
            const expr res = cond{test, conseq, alt};
            return pure(res);
          };
      };
  };


  static maybe<expr> check_record(sexpr::list args) {
    const auto init = just(rec::attr::list());

    // build attribute list by folding args
    return foldr(init, args, [](sexpr e, maybe<rec::attr::list> tail) {
      return tail >> [&](rec::attr::list tail) {
        if(auto self = e.get<sexpr::list>()) {
          return check_binding(*self) >> [&](def d) {
            const rec::attr head{d.name, d.value};
            return just(head >>= tail);
          };
        } else {
          return maybe<rec::attr::list>();
        }
      };
    }) >> [](rec::attr::list attrs) {
      // build record from attribute list, if any
      const expr res = rec{attrs};
      return just(res);
    };
  
  }


  static const auto check_make =
    pop_as<symbol>() >> [](symbol type) {
      return check_record >> [type](expr self) {
        const expr res = make{type, self.cast<rec>().attrs};
        return pure(res);
      };
    };
  
  namespace kw {

    symbol abs("func"),
      let("let"),
      seq("do"),
      def("def"),
      cond("if"),
      rec("record"),
      make("new");
   

    static const std::set<symbol> reserved = {
      abs, let, seq, def, cond, rec, make,
    };

  }
  
  // special forms table
  static const special_type<expr> special_expr = {
    {kw::abs, {check_abs, "(func (`symbol`...) `expr`)"}},
    {kw::let, {check_let, "(let ((`symbol` `expr`)...) `expr`)"}},    
    {kw::seq, {check_seq, "(do ((def `symbol` `expr`) | `expr`)...)"}},
    {kw::cond, {check_cond, "(if `expr` `expr` `expr`)"}},
    {kw::rec, {check_record, "(record (`symbol` `expr`)...)"}},
    {kw::make, {check_make, "(new `symbol` (`symbol` `expr`)...)"}},
  };

  static const special_type<io> special_io = {
    {kw::def, {check_def, "(def `symbol` `expr`)"}},
  };
  

  expr expr::check(const sexpr& e) {
    static const auto impl = check_special(special_expr) | check_call;
    
    return e.match<expr>
      ([](boolean b) { return make_lit(b); },
       [](integer i) { return make_lit(i); },
       [](real r) { return make_lit(r); },
       [](symbol s) -> expr {
         // forbid reserved keywords
         if(kw::reserved.find(s) != kw::reserved.end()) {
           throw error(tool::quote(s.get()) + " is a reserved keyword and"
                       " cannot be used as a variable name");
         }

         // attributes
         if(s.get()[0] == ':') {
           const std::string name = std::string(s.get()).substr(1);
           if(name.empty()) throw error("empty attribute name");
           return sel{name};
         }

         // special cases
         if(s == "true") { return lit<boolean>{true}; }
         if(s == "false") { return lit<boolean>{false}; }
         
         return var{s};
       },
       [](list<sexpr> f) {
         return impl(f).get();
       });
  }
  

  io io::check(const sexpr& e) {
    static const auto impl = check_special(special_io);

    return e.match<io>
      ([](sexpr::list self) -> io {
        if(auto res = impl(self)) return res.get();
        return expr::check(self);
      },[](sexpr self) -> io { return expr::check(self); });
  }

  
  toplevel toplevel::check(const sexpr& e) {
    return io::check(e);
  }

  
  namespace {
    struct repr {
      using type = sexpr;
      
      template<class T>
      sexpr operator()(const lit<T>& self) const {
        return symbol("lit") >>= self.value >>= list<sexpr>();
      }


      sexpr operator()(const var& self) const {
        return symbol("var") >>= self.name >>= list<sexpr>();
      }

      sexpr operator()(const symbol& self) const {
        return self;
      }

      sexpr operator()(const abs::typed& self) const {
        return self.type >>= self.name >>= sexpr::list();
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
          >>= map(self.defs, [](const def& self) -> sexpr {
            return self.name >>= self.value.visit(repr()) >>= sexpr::list();
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
          >>= self.name
          >>= self.value.visit(repr())
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
          >>= self.name
          >>= sexpr::list();        
      }


      sexpr operator()(const rec& self) const {
        return kw::rec
          >>= map(self.attrs, [](rec::attr attr) -> sexpr {
            return attr.name >>= attr.value.visit(repr()) >>= sexpr::list();
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
          >>= self.type
          >>= map(self.attrs, [](rec::attr attr) -> sexpr {
            return attr.name >>= attr.value.visit(repr()) >>= sexpr::list();
          });
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
  
  std::ostream& operator<<(std::ostream& out, const toplevel& self) {
    return out << self.visit(repr());
  }

  
}

