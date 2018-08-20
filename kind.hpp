#ifndef SLAP_KIND_HPP
#define SLAP_KIND_HPP

#include "variant.hpp"
#include "ref.hpp"
#include "symbol.hpp"

namespace kind {

struct constant;
struct constructor;

struct any : variant<ref<constant>, ref<constructor>> {
  using any::variant::variant;

  friend std::ostream& operator<<(std::ostream& out, const any& self);
};

ref<constant> term();
ref<constant> row();

struct constant {
  const symbol name;
};

struct constructor {
  const any from;
  const any to;
};

any operator>>=(any from, any to);

struct error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

}



#endif