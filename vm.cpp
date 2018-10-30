#include "vm.hpp"

namespace vm {

  template<class T>
  static value run(state* self, const T& ) {
    throw std::runtime_error("run unimplemented");
  }


  template<class T>
  static value run(state* self, const ir::lit<T>& expr) {
    return expr.value;
  }


  static value run(state* self, const ir::lit<string>& expr) {
    return make_ref<string>(expr.value);
  }
  
  
  value run(state* self, const ir::expr& expr) {
    return expr.match([&](const auto& expr) {
        return run(self, expr);
      });
  }
  
}
