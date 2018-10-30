#include "vm.hpp"

namespace vm {

  template<class T>
  static value run(state* s, const T& ) {
    throw std::runtime_error("run unimplemented");
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

  
  static value run(state* s, const ir::call& self) {
    const value func = run(s, *self.func);

    const value* sp = s->stack.top();
    
    std::vector<value, stack<value>::allocator> args({s->stack});
    args.reserve(self.args.size());
    
    for(const auto& arg : self.args) {
      args.emplace_back(run(s, arg));
    }

    return func.match([&](const value& ) -> value {
        throw std::runtime_error("type error");
      },
      [&](const ref<closure>& self) {
        // push frame
        s->frames.emplace_back(sp, self->captures.data());

        // evaluate stuff
        value result = run(s, self->body);
        
        // pop frame
        s->frames.pop_back();
        return result;
      });
  }


  static value run(state* s, const ref<ir::closure>& self) {

    std::vector<value> captures;
    captures.reserve(self->captures.size());
    
    for(const ir::expr& c : self->captures) {
      captures.emplace_back(run(s, c));
    }
    
    return make_ref<closure>(captures, self->body);
  }
  
  
  value run(state* s, const ir::expr& self) {
    return self.match([&](const auto& self) {
        return run(s, self);
      });
  }
  
}
