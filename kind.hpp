#ifndef SLAP_KIND_HPP
#define SLAP_KIND_HPP

#include "variant.hpp"
#include "ref.hpp"
#include "symbol.hpp"

namespace kind {

  struct any;
  
  struct constant {
    const symbol name;
    bool operator==(const constant& other) const { return name == other.name; }
  };
  
  struct constructor {
    const ref<any> from;
    const ref<any> to;

    bool operator==(const constructor& other) const;
  };

  struct any : variant<constant, constructor> {
    using any::variant::variant;
  
    friend std::ostream& operator<<(std::ostream& out, const any& self);
  };

  any term();
  any row();
  
  any operator>>=(any from, any to);

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

}



#endif
