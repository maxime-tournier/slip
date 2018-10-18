#ifndef SLAP_REPR_HPP
#define SLAP_REPR_HPP

#include "sexpr.hpp"

namespace ast {
  struct expr;

  sexpr repr(const expr& self);
  
}




#endif
