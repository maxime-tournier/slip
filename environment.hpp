#ifndef SLAP_ENVIRONMENT_HPP
#define SLAP_ENVIRONMENT_HPP

#include <map>
#include <cassert>

#include "ref.hpp"
#include "symbol.hpp"
#include "list.hpp"

#include "tool.hpp"

// environments
template<class T>
struct environment {
  using locals_type = std::map<symbol, T>;
  locals_type locals;
  ref<environment> parent;

  environment(const ref<environment>& parent={}) : parent(parent) { }

  template<class Derived>
  friend ref<Derived> scope(const ref<Derived>& self) {
    assert(self && "null scope parent");
    return make_ref<Derived>(self);
  }
  

  T& find(const symbol& s) {
    auto it = locals.find(s);
    if(it != locals.end()) return it->second;
    
    if(!parent) {
      throw std::out_of_range("unbound variable " + tool::quote(s.get()));
    }
    
    return parent->find(s);
  }

  // convenience
  environment& def(const symbol& s, const T& value) {
    if(!locals.emplace(s, value).second) {
      throw std::runtime_error("redefined variable " + tool::quote(s.get()));
    }
    return *this;
  }

  
  // debugging
  std::size_t write(std::ostream& out, std::size_t level=0) const {
    if(parent) {
      level = parent->write(out, level);
    }

    for(auto& it : locals) {
      out << std::string(level, '.') << it.first << ": " << it.second << std::endl;
    }
    
    return level + 1;
  }
    
  
};




#endif
