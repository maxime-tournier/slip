#ifndef SLAP_REPR_HPP
#define SLAP_REPR_HPP

#include "sexpr.hpp"
#include "ast.hpp"

namespace ast {

  sexpr repr(const expr& self);
  sexpr repr(const abs::arg& self);
  
}




#endif
