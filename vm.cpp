#include "vm.hpp"
#include "sexpr.hpp"

namespace vm {

  state::state(std::size_t size):
    stack(size) {
    frames.reserve(size);    
    frames.emplace_back(stack.next(), nullptr);
  }

  static void run(state* s, const ir::expr& self);  
  
  template<class T>
  static value run(state* s, const T& ) {
    throw std::runtime_error("run unimplemented for: "
                             + tool::type_name(typeid(T)));
  }

  static constexpr bool debug = false;

  template<class ... Args>
  static value* push(state* s, Args&& ... args) {
    value* res = new (s->stack.allocate(1)) value(std::forward<Args>(args)...);
    if(debug) std::clog << "push: " << *res << std::endl;
    return res;
  }

  static value* top(state* s) {
    return s->stack.next() - 1;
  }
  
  static void pop(state* s, std::size_t n=1) {
    for(std::size_t i=0; i < n; ++i) {
      if(debug) std::clog << "pop: " << *(top(s) - i) << std::endl;
      (top(s) - i)->~value();
    }
  }

  
  template<class T>
  static void run(state* s, const ir::lit<T>& self) {
    push(s, self.value);
  }


  static void run(state* s, const ir::lit<string>& self) {
    push(s, make_ref<string>(self.value));
  }


  static void run(state* s, const ir::local& self) {
    assert(s->frames.back().sp);
    push(s, s->frames.back().sp[self.index]);
  }
  
  
  static void run(state* s, const ir::capture& self) {
    assert(s->frames.back().captures);    
    push(s, s->frames.back().captures[self.index]);
  }

  
  static void run(state* s, const ir::global& self) {
    auto it = s->globals.find(self.name);
    assert(it != s->globals.end());
    push(s, it->second);
  }

  // this is the value that will end up in closure captures for recursive calls
  static const value recursive = unit();

  static void run(state* s, const ir::seq& self) {
    push(s, unit());
    
    for(const ir::expr& e: self.items) {
      pop(s);
      run(s, e);
    }
  }

  static void run(state* s, const ref<ir::scope>& self) {

    // push scope defs
    for(const ir::expr& def : self->defs) {
      run(s, def);
    }
    
    // push scope value
    run(s, self->value);

    // fetch result
    value result = std::move(*top(s));
    
    // pop scope size
    pop(s, self->defs.size());
    
    // put back result
    *top(s) = std::move(result);
  }


  static void run(state* s, const ref<ir::cond>& self) {
    // evaluate test
    run(s, self->test);
    const bool test = top(s)->cast<boolean>();
    pop(s);

    if(test) {
      run(s, self->conseq);
    } else {
      run(s, self->alt);
    }
  }

  static value apply(state* s, const builtin& self,
                     const value* args, std::size_t argc);
  static value apply(state* s, const ref<closure>& self,
                     const value* args, std::size_t argc);
  static value apply(state* s, const unit& self,
                     const value* args, std::size_t argc);
  static value apply(state* s, const value& self,
                     const value* args, std::size_t argc);  
  

  static value unsaturated(state* s, const value& self, std::size_t expected,
                           const value* args, std::size_t argc) {
    throw std::runtime_error("unimplemented: unsaturated");
    
    assert(argc != expected);
    if(expected > argc) {
      // under-saturated: close over available arguments + function
      vector<value> captures(args, args + argc);
      captures.emplace_back(self);
      
      // closure call
      vector<ir::expr> args; args.reserve(argc + expected);

      // argc first args come from capture
      for(std::size_t i = 0; i < argc; ++i) {
        args.emplace_back(ir::capture(i));
      }

      // remaining args come from stack
      for(std::size_t i = 0, n = expected - argc; i < n; ++i) {
        args.emplace_back(ir::local(i));
      }
      
      const ir::expr body = make_ref<ir::call>(ir::capture(argc), std::move(args));
      
      return make_ref<closure>(expected - argc, std::move(captures), body);
    } else {
      // over-saturated: call expected arguments then call remaining args
      // regularly
      value tmp = apply(s, self, args, expected);
      return tmp.match([&](const auto& self) {
          return apply(s, self, args + expected, argc - expected);
        });
    }    
  }
  


  // builtin call
  static value apply(state* s, const builtin& self,
                     const value* args, std::size_t argc) {
    if(self.argc != argc) {
      return unsaturated(s, self, self.argc, args, argc);
    }
    
    return self.func(args);
  }

  // closure call
  static value apply(state* s, const ref<closure>& self,
                     const value* args, std::size_t argc) {
    if(self->argc != argc) {
      return unsaturated(s, self, self->argc, args, argc);      
    }

    // push frame
    s->frames.emplace_back(args, self->captures.data(), &self);
    
    // evaluate stuff
    run(s, self->body);

    // pop result
    value result = std::move(*top(s));
    pop(s);

    // sanity check
    assert(s->stack.next() - s->frames.back().sp == argc);
    
    // pop frame
    s->frames.pop_back();
    return result;
  }
  
  // recursive closure call
  static value apply(state* s, const unit&, const value* args, std::size_t argc) {
    assert(s->frames.back().self);
    const ref<closure>* self = s->frames.back().self;
    return apply(s, *self, args, argc);
  }


  template<class T>
  static value apply(state* s, const T& self, const value* args, std::size_t argc) {
    throw std::runtime_error("type error in application: "
                             + tool::type_name(typeid(T)));
  }

  // main dispatch
  static value call(state* s, const value* args, std::size_t argc) {
    const value& self = args[-1];
    
    switch(self.type()) {
    case value::index_of<builtin>::value:
      return apply(s, self.cast<builtin>(), args, argc);
    case value::index_of<ref<closure>>::value:
      return apply(s, self.cast<ref<closure>>(), args, argc);
    case value::index_of<unit>::value:
      return apply(s, unit(), args, argc);
    default: break;
    }
    
    // delegate call based on actual function type
    return self.match([&](const auto& self) {
        return apply(s, self, args, argc);
      });
  }


  static void run(state* s, const ref<ir::call>& self) {
    // evaluate/push function
    run(s, self->func);

    // evaluate/push args
    const value* args = s->stack.next();
    const std::size_t argc = self->args.size();
    for(const auto& arg: self->args) {
      run(s, arg);
    }
    
    // call function: call must cleanup the stack leaving exactly the function
    // arguments in place
    value result = call(s, args, argc);

    // pop arguments
    pop(s, argc);

    // push function result
    *top(s) = std::move(result);
  }


  static void run(state* s, const ref<ir::closure>& self) {

    const value* first = s->stack.next();
    // push captured variables
    for(const ir::expr& c : self->captures) {
      run(s, c);
    }
    
    // TODO move values from the stack into vector
    std::vector<value> captures = {first, first + self->argc};
    
    push(s, make_ref<closure>(self->argc, std::move(captures), self->body));
  }



  static void run(state* s, const ir::import& self) {
    std::clog << "warning: stub impl for import" << std::endl;
  }

  static void run(state* s, const ref<ir::use>& self) {
    std::clog << "warning: stub impl for use" << std::endl;
  }
  
  static void run(state* s, const ir::expr& self) {
    self.match([&](const auto& self) {
      run(s, self);
    });
  }

  ////////////////////////////////////////////////////////////////////////////////
  

  value eval(state* s, const ir::expr& self) {
    run(s, self);
    value res = std::move(*top(s));
    pop(s);
    return res;
  }


  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.match([&](const auto& self) { out << self; },
               [&](const unit& self) { out << "()"; },
               [&](const ref<closure>& ) { out << "#<closure>"; },
               [&](const builtin& ) { out << "#<builtin>"; },
               [&](const boolean& self) { out << (self ? "true" : "false"); },
               [&](const ref<string>& self) { out << '"' << *self << '"';} );
    return out;
  }

  
}
