#ifndef SLAP_SUBSTITUTION_HPP
#define SLAP_SUBSTITUTION_HPP

#include "type.hpp"

namespace type {

  class substitution {
    ref<substitution> parent;

    using key_type = ref<variable>;
    using value_type = mono;
    
    using map_type = std::map<key_type, value_type>;
    map_type map;
  public:

    value_type find(key_type key) const;

    void link(key_type key, value_type value); 

    friend ref<substitution> scope(const ref<substitution>& self);

    void merge();
    
    substitution(const ref<substitution>& parent={});
  };

}


#endif
