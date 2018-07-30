#include "ast.hpp"

#include <map>
#include <functional>

#include "../maybe.hpp"

#include "sexpr.hpp"

namespace ast {

  namespace {
    
    // list destructuring monad: functions that try to extract a value of type U
    // from a list
    template<class U>
    using monad = std::function<maybe<U>(const list<sexpr>& )>;
  
    template<class U>
    static monad<U> pure(const U& value) {
      return [value](const list<sexpr>&) { return value; };
    };

    // matches list head with given type and return it if possible
    template<class U>
    static maybe<U> match(const list<sexpr>& self) {
      if(!self) return {};
      if(auto value = self->head.template get<U>()) {
        return *value;
      }
      return {};
    }
    
    namespace detail {
      template<class T>
      struct maybe_type;
      
      template<class T>
      struct maybe_type<maybe<T>> {
        using type = T;
      };
      
      template<class Func>
      struct monad_type {
        using result_type = typename std::result_of<Func(const list<sexpr>&)>::type;
        using type = typename maybe_type<result_type>::type;
      };
    }

  
    // monadic bind
    template<class A, class F>
    struct bind_type {
      using source_type = typename detail::monad_type<A>::type;
      using result_type = typename std::result_of<F(const source_type&)>::type;
      using value_type = typename detail::monad_type<result_type>::type ;
    
      const A a;
      const F f;
    
      maybe<value_type> operator()(const list<sexpr>& self) const {
        return a(self) >> [&](const source_type& value) {
          return f(value)(self->tail);
        };
      }
    };

    template<class A, class F>
    static bind_type<A, F> operator>>(A a, F f) {
      return {a, f};
    };
  
    // coproduct
    template<class LHS, class RHS>
    struct coproduct_type {
      const LHS lhs;
      const RHS rhs;

      static_assert(std::is_same<typename detail::monad_type<LHS>::type,
                                 typename detail::monad_type<RHS>::type>::value,
                    "value types must agree");

      using value_type = typename detail::monad_type<LHS>::type;

      maybe<value_type> operator()(const list<sexpr>& self) const {
        if(auto value = lhs(self)) {
          return value.get();
        } else if(auto value = rhs(self)){
          return value.get();
        } else {
          return {};
        }
      }
    };

    template<class LHS, class RHS>
    static coproduct_type<LHS, RHS> operator|(LHS lhs, RHS rhs) {
      return {lhs, rhs};
    }

    // fail
    template<class T>
    static maybe<T> fail(const list<sexpr>&) { return {}; }

    struct unimplemented : std::runtime_error {
      unimplemented(const std::string& what)
        : std::runtime_error("unimplemented: " + what) { }
    };
    
  }

  

  // check function calls
  static maybe<expr> check_call(const list<sexpr>& args) {
    if(!args) throw syntax_error("empty list in application");
    
    const auto func = make_ref<expr>(check(args->head));
    const expr res = app{func, map(args->tail, check)};
    return res;
  }

  // check lambdas
  static maybe<expr> check_abs(const list<sexpr>& self) {
    throw unimplemented("abs");
  }

  // special forms table
  static const std::map<symbol, monad<expr>> special = {
    {symbol("func"), check_abs},
  };

  // check lists
  static expr check_list(list<sexpr> f) {

    static const auto impl =
      (match<symbol> >> [](const symbol& s) -> monad<expr> {
        const auto it = special.find(s);
        if(it != special.end()) return it->second;
        return fail<expr>;
      })
      | check_call;
    
    if(auto res = impl(f)) {
      return res.get();
    }

    throw std::runtime_error("derp");
  };
  

  expr check(const sexpr& e) {
    return e.match<expr>
      ([](boolean b) { return make_lit(b); },
       [](integer i) { return make_lit(i); },
       [](real r) { return make_lit(r); },
       [](symbol s) { return var{s}; },
       [](list<sexpr> f) {
         return check_list(f);
       });
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
    
    sexpr operator()(const io& self) const {
      return self.match<sexpr>
        ([](const def& self) {
          return symbol("def")
            >>= self.name
            >>= self.value->visit<sexpr>(repr())
            >>= list<sexpr>();
            },
          [](const ref<expr>& e) {
            return e->visit<sexpr>(repr());
          });
    }
    
  };
  }

  std::ostream& operator<<(std::ostream& out, const expr& self) {
    return out << self.visit<sexpr>(repr());
  }
  
}

