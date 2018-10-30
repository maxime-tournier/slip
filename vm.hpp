#ifndef SLIP_VM_HPP
#define SLIP_VM_HPP

#include "eval.hpp"
#include "stack.hpp"

#include "ir.hpp"

namespace vm {

  using builtin = eval::closure;
  
  struct value;
  struct closure;
  
  struct value : variant<unit, boolean, integer, real, ref<string>, builtin, ref<closure>> {
    using value::variant::variant;
  };
                         
  struct closure {
    const std::vector<value> captures;
    const ir::expr body;
    closure(std::vector<value> captures, const ir::expr& body)
      : captures(std::move(captures)),
        body(body) { }
  };

  
  struct frame {
    const value* sp;
    const value* captures;
    frame(const value* sp, const value* captures)
      : sp(sp),
        captures(captures) { }
  };
  

  struct state {
    class stack<value> stack;
    std::vector<frame> frames;
    std::map<symbol, value> globals;
  };


  value run(state* self, const ir::expr& expr);
  
}



#endif
