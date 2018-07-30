#ifndef AST_HPP
#define AST_HPP

#include "ref.hpp"
#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "base.hpp"

struct sexpr;

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
    const ref<expr> body;
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

    friend std::ostream& operator<<(std::ostream& out, const expr& self);
  };


  expr check(const sexpr& e);

  struct syntax_error : std::runtime_error {
    using runtime_error::runtime_error;
  };
  
  
}


#endif
