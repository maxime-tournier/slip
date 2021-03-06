#include "kind.hpp"

#include <ostream>

namespace kind {

  // constants
  any term() { return constant{"*"}; }
  any row() { return constant{"..."}; }  
  
  any operator>>=(any from, any to) {
    return constructor{make_ref<any>(from), make_ref<any>(to)};
  }

  bool constructor::operator==(const constructor& other) const {
    return *from == *other.from && *to == *other.to;
  }  

  namespace {
    struct ostream_visitor {
      using type = void;
  
      void operator()(const constant& self, std::ostream& out, bool ) const {
        out << self.name;
      }
      
      void operator()(const constructor& self, std::ostream& out, bool parens) const {
        if(parens) out << "(";
        self.from->visit(ostream_visitor(), out, true);
        out << " -> ";
        self.to->visit(ostream_visitor(), out, false);
        if(parens) out << ")";
      }
        
    };
  }

  std::ostream& operator<<(std::ostream& out, const any& self) {
    self.visit(ostream_visitor(), out, false);
    return out;
  }
  
}
