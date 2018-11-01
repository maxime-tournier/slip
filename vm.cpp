#include "vm.hpp"
#include "sexpr.hpp"

namespace vm {

  state::state(std::size_t size):
    stack(size) {
    frames.reserve(size);    
    frames.emplace_back(stack.top(), nullptr);
  }
  
  template<class T>
  static value run(state* s, const T& ) {
    throw std::runtime_error("run unimplemented for: "
                             + tool::type_name(typeid(T)));
  }


  template<class T>
  static value run(state* s, const ir::lit<T>& expr) {
    return expr.value;
  }


  static value run(state* s, const ir::lit<string>& expr) {
    return make_ref<string>(expr.value);
  }


  static value run(state* s, const ir::local& self) {
    assert(s->frames.back().sp);
    return s->frames.back().sp[self.index];
  }
  
  
  static value run(state* s, const ir::capture& self) {
    assert(s->frames.back().captures);    
    return s->frames.back().captures[self.index];
  }

  
  static value run(state* s, const ir::global& self) {
    auto it = s->globals.find(self.name);
    assert(it != s->globals.end());
    return it->second;
  }

  // this is the value that will end up in closure captures for recursive calls
  static const value recursive = unit();

  static value run(state* s, const ref<ir::push>& self) {
    value* storage = new (s->stack.allocate(1)) value(recursive);
    *storage = run(s, self->value);
    return unit();
  }
  
  static value run(state* s, const ir::seq& self) {
    value res = unit();
    for(const ir::expr& e: self.items) {
      res = run(s, e);
    }
    return res;
  }

  static value run(state* s, const ref<ir::scope>& self) {
    value* sp = s->stack.top();
    value res = run(s, self->value);

    // destruct values
    for(std::size_t i = self->size; i > 0; --i) {
      sp[i - 1].~value();
    }
    
    s->stack.deallocate(sp, self->size);
    return res;
  }


  static value run(state* s, const ref<ir::cond>& self) {
    if(run(s, self->test).cast<boolean>()) {
      return run(s, self->conseq);
    } else {
      return run(s, self->alt);
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
    value result = run(s, self->body);

    // TODO exception safety on frames?
    
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
  static value apply(state* s, const value& self,
                     const value* args, std::size_t argc) {

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

  
  static value run(state* s, const ref<ir::call>& self) {
    // evaluation function
    const value func = run(s, self->func);

    // allocate temporary stack space for args
    std::vector<value, stack<value>::allocator> args({s->stack});
    args.reserve(self->args.size());

    // evaluate args
    for(const auto& arg: self->args) {
      args.emplace_back(run(s, arg));
    }

    // call
    return apply(s, func, args.data(), args.size());
  }


  static value run(state* s, const ref<ir::closure>& self) {
    // compute captured variables
    std::vector<value> captures;
    captures.reserve(self->captures.size());
    
    for(const ir::expr& c : self->captures) {
      captures.emplace_back(run(s, c));
    }
    
    return make_ref<closure>(self->argc, captures, self->body);
  }



  static value run(state* s, const ir::import& self) {
    // TODO
    std::clog << "warning: stub impl for import" << std::endl;
    return unit();
  }

  static value run(state* s, const ref<ir::use>& self) {
    // TODO
    std::clog << "warning: stub impl for use" << std::endl;
    return unit();
  }
  

  ////////////////////////////////////////////////////////////////////////////////
  
  value run(state* s, const ir::expr& self) {
    return self.match([&](const auto& self) {
        return run(s, self);
      });
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
