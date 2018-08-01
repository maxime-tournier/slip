#include "ast.hpp"

#include <map>
#include <set>

#include <functional>

#include "../maybe.hpp"

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
        const syntax_error err(it->second.second);
        return it->second.first | error<U>(err);
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
    if(!args) throw syntax_error("empty list in application");
    
    const auto func = make_ref<expr>(expr::check(args->head));
    const expr res = app{func, map(args->tail, expr::check)};
    return res;
  }

  // check lambdas
  using args_type = list<symbol>;
  
  static maybe<args_type> check_args(sexpr::list self) {
    maybe<args_type> init(nullptr);
    return foldr(init, self, [](sexpr lhs, maybe<args_type> rhs) {
                               return rhs >> [&](args_type tail) -> maybe<args_type> {
                                 if(auto res = lhs.get<symbol>()) return *res >>= tail;
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
             return empty(pure(e));
           } else {
             return fail<expr>();
           }
         };
       };


  static const auto check_def = pop_as<symbol>()
    >> [](symbol name) {
         return pop() >> [name](sexpr value) {
                         const io res = def{name, expr::check(value)};
                         return empty(pure(res));
                       };
       };
  

  static maybe<expr> check_seq(sexpr::list args) {
    const expr res = seq{map(args, io::check)};
    return res;
  }
  
  namespace kw {
    symbol abs("func"), seq("do"), def("def");

    static const std::set<symbol> reserved = {
                                              abs, seq, def,
    };
  }
  
  // special forms table
  static const special_type<expr> special_expr = {
                                                  {kw::abs, {check_abs, "(func (`symbol`...) `expr`)"}},
                                                  {kw::seq, {check_seq, "(do ((def `symbol` `expr`) | `expr`)...)"}},
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
         if(kw::reserved.find(s) != kw::reserved.end()) {
           throw syntax_error(tool::quote(s.get()) + " is a reserved keyword and"
                              " cannot be used as a variable name");
         }
         
         if(s.get()[0] == '@') {
           const std::string name = std::string(s.get()).substr(1);
           if(name.empty()) throw syntax_error("empty attribute name");
           return attr{symbol(name)};
         }
         
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

      template<class T>
      sexpr operator()(const lit<T>& self) const {
        return symbol("lit") >>= self.value >>= list<sexpr>();
      }

      sexpr operator()(const var& self) const {
        return symbol("var") >>= self.name >>= list<sexpr>();
      }

      sexpr operator()(const abs& self) const {
        return symbol("abs")
          >>= map(self.args, [](symbol s) -> sexpr { return s;})
          >>= self.body->visit<sexpr>(repr())
          >>= list<sexpr>();
      }

      sexpr operator()(const app& self) const {
        return symbol("app")
          >>= self.func->visit<sexpr>(repr())
          >>= map(self.args, [](const expr& e) { return e.visit<sexpr>(repr()); })
          >>= list<sexpr>();
      }

      sexpr operator()(const seq& self) const {
        return symbol("seq")
          >>= map(self.items, repr())
          >>= list<sexpr>();
      }


      sexpr operator()(const def& self) const {
        return symbol("def")
          >>= self.name
          >>= self.value.visit<sexpr>(repr())
          >>= list<sexpr>();
      }
    
      sexpr operator()(const io& self) const {
        return self.visit<sexpr>(repr());
      }

      sexpr operator()(const expr& self) const {
        return self.visit<sexpr>(repr());
      }

      sexpr operator()(const attr& self) const {
        return symbol("attr")
          >>= self.name
          >>= sexpr::list();        
      }

      template<class T>
      sexpr operator()(const T& self) const {
        throw std::logic_error("derp");
      }
    
    };
  }

  std::ostream& operator<<(std::ostream& out, const expr& self) {
    return out << self.visit<sexpr>(repr());
  }


  std::ostream& operator<<(std::ostream& out, const io& self) {
    return out << self.visit<sexpr>(repr());
  }
  
  std::ostream& operator<<(std::ostream& out, const toplevel& self) {
    return out << self.visit<sexpr>(repr());
  }

  
}

