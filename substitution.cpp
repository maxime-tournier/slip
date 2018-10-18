#include "substitution.hpp"

namespace type {

  substitution::value_type substitution::find(key_type key) const {
    auto it = map.find(key);
    if(it != map.end()) return it->second;

    if(parent) return parent->find(key);
    
    return key;
  }

  void substitution::link(key_type key, value_type value) {
    auto it = map.emplace(key, value);
    assert(it.second); (void) it;
  }

  void substitution::merge() {
    assert(parent);
      
    for(auto& it : map) {
      parent->link(it.first, it.second);
    }
  }
  
  ref<substitution> scope(const ref<substitution>& self) {
    return make_ref<substitution>(self);
  }

  substitution::substitution(const ref<substitution>& parent)
    : parent(parent) { }
}
