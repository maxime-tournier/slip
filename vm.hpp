#ifndef SLIP_VM_HPP
#define SLIP_VM_HPP

#include "eval.hpp"
#include "stack.hpp"

#include "ir.hpp"

namespace vm {

  struct value;
  struct closure;

  struct builtin {
    std::size_t argc;
    using func_type = value (*)(const value* args);
    func_type func;

    builtin(std::size_t argc, func_type func):
      argc(argc),
      func(func) { }


    template<class Func>
    explicit builtin(Func func);
  };


  struct value : variant<unit, boolean, integer, real, ref<string>, builtin, ref<closure>> {
    using value::variant::variant;

    friend std::ostream& operator<<(std::ostream& out, const value& self);
  };


                         
  struct closure {
    const std::size_t argc;
    const std::vector<value> captures;
    const ir::expr body;
    
    closure(std::size_t argc, std::vector<value> captures, const ir::expr& body):
      argc(argc),
      captures(std::move(captures)),
      body(body) { }
  };

  
  struct frame {
    const value* sp;
    const value* captures;
    const ref<closure>* self;
    
    frame(const value* sp, const value* captures, const ref<closure>* self=nullptr):
      sp(sp),
      captures(captures),
      self(self) { }
  };
  

  struct state {
    class stack<value> stack;
    std::vector<frame> frames;
    std::map<symbol, value> globals;

    state(std::size_t size);

    state& def(symbol name, value global) {
      globals.emplace(name, global);
      return *this;
    }

  };


  value eval(state* self, const ir::expr& expr);


  template<class Func, class Ret, class ... Args>
  static builtin from_lambda(Func func, Ret (*)(const Args&...)) {
    static Func instance(func);
    return builtin(sizeof...(Args), [](const value* args) -> value {
        const std::tuple<const Args&...> unpack = { (args++)->cast<Args>()... };
        return tool::apply(instance, unpack, std::index_sequence_for<Args...>());
      });
  }

  template<class Func>
  builtin::builtin(Func func):
    builtin(from_lambda(func, +func)) { }
     
}



#endif
