#ifndef SLAP_INFER_HPP
#define SLAP_INFER_HPP

#include "type.hpp"

namespace ast {
  struct expr;
};

namespace type {

  mono infer(const ref<state>& s, const ast::expr& self);
  
}


#endif
