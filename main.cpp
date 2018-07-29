#include "variant.hpp"
#include "list.hpp"

// symbol
#include <set>

// refs
#include <memory>

//
#include <map>

#include <iostream>

#include "symbol.hpp"
#include "environment.hpp"

#include "../parse.hpp"

#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

// base types
using boolean = bool;
using integer = long;
using real = double;


// s-expressions
struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
};


static sexpr parse(std::istream& in) {
  using parser::pure;
  
  static const auto true_parser =
    parser::token("true") >> [](const char*) { return pure(true); };
  
  static const auto false_parser =
    parser::token("false") >> [](const char*) { return pure(false); };
  
  static const auto boolean_parser = true_parser | false_parser;

  static const auto real_parser = parser::value<double>();  

  static const auto number_parser =
    real_parser >> [](real num) {
                     const long cast = num;
                     const sexpr value = num == real(cast) ? cast : num;
                     return pure(value);                     
                   };
  
  static const auto initial_parser = parser::chr<std::isalpha>();
  static const auto rest_parser = parser::chr<std::isalnum>();  
  
  static const auto symbol_parser =
    initial_parser >>
    [](char c) {
      return parser::noskip(*rest_parser >>
                            [c](std::vector<char>&& rest) {
                              const std::string tmp = c + std::string(rest.begin(),
                                                                      rest.end());
                              return pure(symbol(tmp));
                            });
    };
  
  static const auto as_expr = parser::cast<sexpr>();

  static auto expr_parser = parser::any<sexpr>();

  static const auto separator_parser = +parser::chr<std::isspace>();
  
  static const auto list_parser
    = parser::token("(")
    >>= ((parser::ref(expr_parser) % separator_parser)
         | pure(std::vector<sexpr>()))
    >> drop(parser::token(")"))
    >> [](std::vector<sexpr>&& es) {
         return pure(make_list(es.begin(), es.end()));
       };
  
  static const auto once =
    (expr_parser
     = (boolean_parser >> as_expr)
     | (symbol_parser >> as_expr)
     | number_parser
     | (list_parser >> as_expr)
     , 0); (void) once;
  
  if(auto value = expr_parser(in)) {
    return value.get();
  }
  
  throw std::runtime_error("parse error");
};



namespace ast {

  // expressions
  template<class T>
  struct lit {
    const T value;
  };

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


  // definitions
  struct def {
    const symbol name;
    const ref<expr> value;
  };


  // sequence
  struct seq {
    const ref<expr> items;        
  };


  struct expr : variant< lit<boolean>,
                         lit<integer>,
                         lit<real>,
                         var, abs, app,
                         def,
                         seq> {
    using expr::variant::variant;
  };


  static expr check(const sexpr& e);
  
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
