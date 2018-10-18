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


  struct substitute_visitor {

    mono operator()(const ref<variable>& self, const substitution& sub) const {
      const mono t = sub.find(self);
      if(t == self) return t;

      return sub.substitute(t);
    }

    mono operator()(const ref<application>& self, const substitution& sub) const {
      return sub.substitute(self->ctor)(sub.substitute(self->arg));
    }

    mono operator()(const ref<constant>& self, const substitution&) const {
      return self;
    }
  
  };

  mono substitution::substitute(const mono& t) const {
    return t.visit(substitute_visitor(), *this);
  }


  
}
