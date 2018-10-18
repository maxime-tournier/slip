#ifndef SLAP_UNIFY_HPP
#define SLAP_UNIFY_HPP

#include "type.hpp"

namespace type {

  void unify_terms(state* self, substitution* sub,
                   mono from, mono to, logger* log=nullptr);
  
  void unify_rows(state* self, substitution sub,
                  const app& from, const app& to, logger* log=nullptr);
  
}


#endif
