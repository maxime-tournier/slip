#include "vm.hpp"

namespace vm {

  state::state(std::size_t size):
    stack(size) {
    frames.emplace_back(stack.top(), nullptr);
  }
  
  template<class T>
  static value run(state* s, const T& ) {
    throw std::runtime_error("run unimplemented for: " + tool::type_name(typeid(T)));
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

    // destruct
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


  static value call(state* s, const value* sp, const closure& self) {
    // push frame
    s->frames.emplace_back(sp, self.captures.data(), &self);
    
    // evaluate stuff
    value result = run(s, self.body);

    // TODO exception safety
    
    // pop frame
    s->frames.pop_back();
    return result;
  }
  
  static value run(state* s, const ref<ir::call>& self) {
    const value func = run(s, self->func);

    // TODO handle saturated/unsaturated calls
    const std::size_t argc = func.match([&](const auto& self) -> std::size_t {
        throw std::runtime_error("type error in application: " + tool::type_name(typeid(self)));
      },
      [&](const ref<closure>& self) { return self->argc; },
      [&](const builtin& self) { return self.argc; },
      [&](const unit& self) {
        assert(s->frames.back().self);
        return s->frames.back().self->argc;
      });

    if(argc != self->args.size()) {
      throw std::runtime_error("unimplemented: non-saturated calls");
    }
    
    // stack pointer
    const value* sp = s->stack.top();

    // allocate temporary stack space for args
    std::vector<value, stack<value>::allocator> storage({s->stack});
    storage.reserve(self->args.size());
    
    for(const auto& arg: self->args) {
      storage.emplace_back(run(s, arg));
    }
    
    return func.match([&](const auto& self) -> value {
        throw std::runtime_error("type error in application: " + tool::type_name(typeid(self)));
      },
      [&](const builtin& self) {
        return self.func(sp);
      },
      [&](const ref<closure>& self) {
        return call(s, sp, *self);
      },
      [&](const unit& self) {
        assert(s->frames.back().self);
        return call(s, sp, *s->frames.back().self);
      });
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
