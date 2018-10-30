#ifndef SLIP_VM_HPP
#define SLIP_VM_HPP

#include "eval.hpp"
#include "stack.hpp"

#include "ir.hpp"

namespace vm {

  using value = eval::value;
  
  struct frame {
    const value* sp;
    const value* captures;
  };
  

  struct state {
    class stack<value> stack;
    std::vector<frame> frames;
    std::map<symbol, value> globals;
  };


  value run(state* self, const ir::expr& expr);
  
}



#endif
