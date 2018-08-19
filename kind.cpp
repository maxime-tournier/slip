#include "kind.hpp"

#include <ostream>

namespace kind {

// constants
const ref<constant> term = make_ref<constant>(constant{symbol{"*"}});
const ref<constant> row = make_ref<constant>(constant{symbol{"row"}});

any operator>>=(any from, any to) {
  return make_ref<constructor>(constructor{from, to});
}


  namespace {
    struct ostream_visitor {

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
