#ifndef SLIP_IR_HPP
#define SLIP_IR_HPP

#include "variant.hpp"
#include "ref.hpp"

#include "base.hpp"
#include "symbol.hpp"
#include "string.hpp"
#include "vector.hpp"

#include <map>

struct sexpr;

namespace ast {
  struct expr;
}

namespace ir {

  struct expr;

  struct closure;
  struct branch;
  struct call;

  
  // local variable access
  struct local {
    std::size_t index;
    inline local(std::size_t index) : index(index) { }
  };

  
  // capture variable access
  struct capture {
    std::size_t index;
    inline capture(std::size_t index) : index(index) { }
  };
  

  // global variable access
  struct global {
    symbol name;
  };


  // global def
  struct def {
    symbol name;
  };
  
  template<class T>
  struct lit {
    const T value;
  };

  struct drop { std::size_t count=1; };

  struct block {
    vector<expr> items;
  };

  // scope exit: pop result, pop locals and push result back
  struct exit {
    std::size_t locals;
  };
  
  // attribute selection
  struct sel {
    symbol attr;
  };

  struct import {
    const symbol package;
  };

  struct call {
    std::size_t argc;
  };
  
  // TODO this one needs help from the typechecker
  struct use;
  
  struct expr : variant<lit<unit>, lit<boolean>, lit<integer>, lit<real>, lit<string>,
                        local, capture, global,
                        call,
                        ref<closure>,
                        block, exit, drop, 
                        ref<branch>,
                        import, ref<use>,
                        def, sel> {
    using expr::variant::variant;
  };
  

  
  struct use {
    const expr env;
    use(expr env):
      env(env) { }
  };
  

  struct closure {
    const std::size_t argc;
    const vector<expr> captures;
    const block body;

    closure(std::size_t argc, vector<expr> captures, block body);
  };

  
  struct branch {
    const expr then;
    const expr alt;
    
    branch(expr then, expr alt):
      then(then),
      alt(alt) { }
  };

  
  // toplevel compilation
  expr compile(const ast::expr& self);


  // 
  sexpr repr(const expr& self);
  
}


#endif
