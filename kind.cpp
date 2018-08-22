#include "kind.hpp"

#include <ostream>

namespace kind {

// constants
ref<constant> term() {
  static const auto res = make_ref<constant>(constant{symbol{"*"}});
  return res;
}

ref<constant> row() {
  static const auto res = make_ref<constant>(constant{symbol{"@"}});
  return res;
}


any operator>>=(any from, any to) {
  return make_ref<constructor>(constructor{from, to});
}
  

namespace {
struct ostream_visitor {
  using type = void;
  
  void operator()(const ref<constant>& self, std::ostream& out) const {
    out << self->name;
  }
      
  void operator()(const ref<constructor>& self, std::ostream& out) const {
    out << self->from << " -> " << self->to;           
  }
        
};
}

  
std::ostream& operator<<(std::ostream& out, const any& self) {
  self.visit(ostream_visitor(), out);
  return out;
}
  
}
