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
  
  struct local {
    std::size_t index;
    inline local(std::size_t index) : index(index) { }
  };

  
  struct capture {
    std::size_t index;
    inline capture(std::size_t index) : index(index) { }
  };


  struct call {
    const ref<expr> func;
    const std::vector<expr> args;
  };

  
  template<class T>
  struct lit {
    const T value;
  };
  
  
  struct expr : variant<lit<unit>, lit<boolean>, lit<integer>, lit<real>, lit<string>,
                        local, capture,
                        ref<closure>, call> {
    using expr::variant::variant;
  };


  struct closure {
    const std::size_t argc;
    const std::vector<expr> captures;
    const expr body;

    closure(std::size_t argc, std::vector<expr> captures, expr body);
  };

  
  struct state {
    ref<state> parent;
    inline state(ref<state> parent={}) : parent(parent) { }
    
    std::map<symbol, local> locals;
    std::map<symbol, capture> captures;

    state& def(symbol name);
    
    expr find(symbol name);

    inline friend ref<state> scope(ref<state> self) {
      return make_ref<state>(self);
    }
    
  };

  expr compile(ref<state> ctx, const ast::expr& self);
  
}


#endif
