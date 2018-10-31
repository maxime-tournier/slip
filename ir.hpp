#ifndef SLIP_IR_HPP
#define SLIP_IR_HPP

#include "variant.hpp"
#include "ref.hpp"

#include "base.hpp"
#include "symbol.hpp"
#include "string.hpp"

#include <vector>
#include <map>

namespace ast {
  struct expr;
}

namespace ir {

  struct expr;

  struct closure;
  struct cond;
  struct push;
  struct scope;
  
  struct local {
    std::size_t index;
    inline local(std::size_t index) : index(index) { }
  };


  struct capture {
    std::size_t index;
    inline capture(std::size_t index) : index(index) { }
  };

  struct global {
    symbol name;
  };
  
  struct call {
    const ref<expr> func;
    const std::vector<expr> args;
  };

  
  template<class T>
  struct lit {
    const T value;
  };

  struct seq {
    std::vector<expr> items;
  };

  struct import {
    const symbol package;
  };

  struct use;
  
  struct expr : variant<lit<unit>, lit<boolean>, lit<integer>, lit<real>, lit<string>,
                        local, capture, global,
                        ref<scope>, ref<push>,
                        ref<closure>, call,
                        seq,
                        ref<cond>,
                        import, ref<use>> {
    using expr::variant::variant;
  };

  struct use {
    const expr env;
    use(expr env):
      env(env) { }
  };
  
  struct push {
    const expr value;

    push(expr value):
      value(value) { }
  };
  
  struct scope {
    const std::size_t size;
    const expr body;
    scope(std::size_t size, expr body):
      size(size),
      body(body) { }
  };

  
  struct closure {
    const std::size_t argc;
    const std::vector<expr> captures;
    const expr body;

    closure(std::size_t argc, std::vector<expr> captures, expr body);
  };

  struct cond {
    const expr test;
    const expr conseq;
    const expr alt;
    cond(expr test, expr conseq, expr alt)
      : test(test), conseq(conseq), alt(alt) { }
  };
  
  // toplevel compilation
  expr compile(const ast::expr& self);
  
}


#endif
