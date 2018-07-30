#include <map>
#include <iostream>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "sexpr.hpp"
#include "environment.hpp"

#include "../maybe.hpp"


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


  template<class Ret, class T>
  static std::function< maybe<Ret>(list<sexpr>) >
  unpack(std::function<Ret(const T&...)> cont) {
    return [cont](list<sexpr> e) -> maybe<Ret> {
      if(!e) return {};
      if(auto h = e->head.get<T>()) {
        return cont(*h)(e->tail);
      }
      return {};
    };
  };

  static expr check_expr(list<sexpr> f);
  
  static expr check(const sexpr& e) {
    return e.match<expr>
      ([](boolean b) { return make_lit(b); },
       [](integer i) { return make_lit(i); },
       [](real r) { return make_lit(r); },
       [](symbol s) { return var{s}; },
       [](list<sexpr> f) {
         return check_expr(f);
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


static std::runtime_error unimplemented() {
  return std::runtime_error("unimplemented");
}


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
