#ifndef SLIP_VM_HPP
#define SLIP_VM_HPP

#include "eval.hpp"
#include "stack.hpp"

#include "ir.hpp"

namespace vm {

  struct tag;
  using gc = class gc<tag>;
  
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

  struct record {
    std::map<symbol, value> attrs;

    record(std::map<symbol, value> attrs);
    ~record();
  };

  
  struct value : variant<unit, boolean, integer, real, gc::ref<string>, builtin,
                         list<value>,
                         gc::ref<closure>, gc::ref<record>> {
    using value::variant::variant;

    friend std::ostream& operator<<(std::ostream& out, const value& self);
  };


                         
  struct closure {
    const std::size_t argc;
    const ir::block body;
    
    std::vector<value> captures;
    
    closure(std::size_t argc,
            ir::block body,
            std::vector<value> captures={}):
      argc(argc),
      body(std::move(body)),
      captures(std::move(captures)){ }
  };

  
  struct frame {
    const value* sp;            // frame start
    const value* cp;            // capture pointer
    
    frame(const value* sp,
          const value* cp):
      sp(sp),
      cp(cp) { }
  };
  

  struct state {
    class stack<value> stack;
    std::vector<frame> frames;

    state(const state&) = delete;
    state(state&&) = default;
    
    std::map<symbol, value> globals;

    state(std::size_t size=1000);

    state& def(symbol name, value global) {
      globals.emplace(name, global);
      return *this;
    }

  };

  void collect(state* self);
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
