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
  struct cond;
  struct scope;
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
  struct def;
  
  template<class T>
  struct lit {
    const T value;
  };
  

  struct seq {
    vector<expr> items;
  };

  
  struct import {
    const symbol package;
  };

  
  struct use;
  
  struct expr : variant<lit<unit>, lit<boolean>, lit<integer>, lit<real>, lit<string>,
                        local, capture, global,
                        ref<scope>, ref<closure>, ref<call>,
                        seq,
                        ref<cond>,
                        import, ref<use>,
                        ref<def>> {
    using expr::variant::variant;
  };
  

  struct call {
    const expr func;
    const vector<expr> args;

    call(expr func, vector<expr> args):
      func(func),
      args(std::move(args)) { }
  };


  
  struct use {
    const expr env;
    use(expr env):
      env(env) { }
  };
  

  // 
  struct scope {
    const vector<expr> defs;
    const expr value;
    
    scope(vector<expr> defs, expr value):
      defs(std::move(defs)),
      value(value) { }
  };

  
  struct closure {
    const std::size_t argc;
    const vector<expr> captures;
    const expr body;

    closure(std::size_t argc, vector<expr> captures, expr body);
  };

  
  struct cond {
    const expr test;
    const expr conseq;
    const expr alt;
    
    cond(expr test, expr conseq, expr alt):
      test(test),
      conseq(conseq),
      alt(alt) { }
  };

 struct def {
    symbol name;
    expr value;

    def(symbol name,
        expr value)
      : name(name),
        value(value) {

    }
 };
  
  
  // toplevel compilation
  expr compile(const ast::expr& self);


  // 
  sexpr repr(const expr& self);
  
}


#endif
