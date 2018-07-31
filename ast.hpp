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

  // attribute access
  struct attr {
    const symbol name;
  };

  struct io;
  
  // computation sequencing
  struct seq {
    const list<io> items;
  };

  struct expr : variant<lit<boolean>,
                        lit<integer>,
                        lit<real>,
                        var, abs, app,
                        seq,
                        attr> {
    using expr::variant::variant;

    friend std::ostream& operator<<(std::ostream& out, const expr& self);

    static expr check(const sexpr& e);
  };

  // definition
  struct def {
    const symbol name;
    const expr value;
  };
  
  // stateful computations
  struct io : variant<def, expr>{
    using io::variant::variant;

    static io check(const sexpr& e);
    friend std::ostream& operator<<(std::ostream& out, const io& self);    
  };


  struct toplevel : variant<io> {
    using toplevel::variant::variant;
    
    // TODO types, modules etc
    friend std::ostream& operator<<(std::ostream& out, const toplevel& self);

    static toplevel check(const sexpr& e);
  };

  
  struct syntax_error : std::runtime_error {
    using runtime_error::runtime_error;
  };
  
}


#endif
