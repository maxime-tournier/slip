#include <map>
#include <iostream>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "sexpr.hpp"
#include "environment.hpp"

#include <functional>
#include "../maybe.hpp"


struct unimplemented : std::runtime_error {
  unimplemented(const std::string& what)
    : std::runtime_error("unimplemented: " + what) { }
};

namespace ast {

  // expressions
  template<class T>
  struct lit {
    const T value;
  };

  template<class T>
  static lit<T> make_lit(T value) { return {value}; }

  struct expr;

  struct app {
    const ref<expr> func;
    const list<expr> args;
  };

  struct abs {
    const list<symbol> args;
    const list<expr> body;
  };

  struct var {
    const symbol name;
  };


  // definition
  struct def {
    const symbol name;
    const ref<expr> value;
  };
  
  // stateful computations
  struct io : variant<def, ref<expr> >{
    using io::variant::variant;
  };

  // computation sequencing
  struct seq {
    const list<io> items;
  };

  struct expr : variant< lit<boolean>,
                         lit<integer>,
                         lit<real>,
                         var, abs, app,
                         def,
                         seq> {
    using expr::variant::variant;
  };

  // list destructuring monad
  template<class U>
  using monad = std::function<maybe<U>(const list<sexpr>& )>;
  
  template<class U>
  static monad<U> pure(const U& value) {
    return [value](const list<sexpr>&) { return value; };
  };

  // matches list head with given type and return it (maybe)
  template<class U>
  static maybe<U> match(const list<sexpr>& self) {
    if(!self) return {};
    if(auto value = self->head.template get<U>()) {
      return *value;
    }
    return {};
  }


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

  
  // monadic bind
  template<class A, class F>
  struct bind_type {
    using source_type = typename monad_type<A>::type;
    using result_type = typename std::result_of<F(const source_type&)>::type;
    using value_type = typename monad_type<result_type>::type ;
    
    const A a;
    const F f;
    
    maybe<value_type> operator()(const list<sexpr>& self) const {
      return a(self) >> [&](const source_type& value) {
        return f(value)(self->tail);
      };
    }
  };

  template<class A, class F>
  static bind_type<A, F> bind(A a, F f) {
    return {a, f};
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

    static_assert(std::is_same<typename monad_type<LHS>::type, typename monad_type<RHS>::type>::value,
                  "value types must agree");

    using value_type = typename monad_type<LHS>::type;

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
  
  static expr check(const sexpr& e);

  struct syntax_error : std::runtime_error {
    using runtime_error::runtime_error;
  };
  
  static maybe<expr> check_call(const list<sexpr>& args) {
    if(!args) throw syntax_error("empty list in application");

    const auto func = make_ref<expr>(check(args->head));
    const expr res = app{func, map(args->tail, check)};
    return res;
  }

  
  static maybe<expr> check_abs(const list<sexpr>& self) {
    throw unimplemented("abs");
  }
  
  static const std::map<symbol, monad<expr>> special = {
    {symbol("func"), check_abs},
  };
  
  static expr check_list(list<sexpr> f) {
    if(!f) throw syntax_error("empty list in application");

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
  
  static expr check(const sexpr& e) {
    return e.match<expr>
      ([](boolean b) { return make_lit(b); },
       [](integer i) { return make_lit(i); },
       [](real r) { return make_lit(r); },
       [](symbol s) { return var{s}; },
       [](list<sexpr> f) {
         return check_list(f);
       });
  }
  
}


struct value;
using env = environment<value>;


// values
struct lambda {
  const list<symbol> args;
  const sexpr body;
  const ref<env> scope;

  friend std::ostream& operator<<(std::ostream& out, const lambda& self) {
    return out << "#<lambda>";
  }
};


struct value : variant<real, integer, boolean, symbol, lambda, list<value> > {
  using value::variant::variant;
  using list = list<value>;
};





using eval_type = value (*)(const ref<env>&, const sexpr& );

static std::map<symbol, eval_type> special;




static value eval(const ref<env>& e, const sexpr& expr);


static value apply(const ref<env>& e, const sexpr& func, const list<sexpr>& args) {
  return eval(e, func).match<value>
    ([](value) -> value {
       throw std::runtime_error("invalid type in application");
     },
     [&](const lambda& self) {
       return eval(augment(e, self.args, map(args, [&](const sexpr& it) {
                                                     return eval(e, it);
                                                   })), self.body);
     });
}


static value eval(const ref<env>& e, const sexpr& expr) {
  return expr.match<value>
    ([](value self) { return self; },
     [&](symbol self) {
       auto it = e->locals.find(self);
       if(it == e->locals.end()) {
         throw std::runtime_error("unbound variable: " + std::string(self.get()));
       }

       return it->second;
     },
     [&](const list<sexpr>& x) {
       if(!x) throw std::runtime_error("empty list in application");

       // special forms
       if(auto s = x->head.get<symbol>()) {
         auto it = special.find(*s);
         if(it != special.end()) {
           return it->second(e, x->tail);
         }
       }

       // regular apply
       return apply(e, x->head, x->tail);       
     });
}


template<class F>
static void read_loop(const F& f) {
  // TODO read history
  while(const char* line = readline("> ")) {
    if(!*line) continue;
    
    add_history(line);
	std::stringstream ss(line);
    
	f(ss);
  }
  
};



int main(int, char**) {

  read_loop([](std::istream& in) {
              try {
                const sexpr s = parse(in);
                std::cout << s << std::endl;
              } catch(std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
              }
            });
  
  return 0;
}
