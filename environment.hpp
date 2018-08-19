#ifndef SLAP_ENVIRONMENT_HPP
#define SLAP_ENVIRONMENT_HPP

#include <map>

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
    return make_ref<Derived>(self);
  }
  
  template<class Derived, class Symbols, class Iterator>
  friend ref<Derived> augment(const ref<Derived>& self,
                              const Symbols& symbols,
                              Iterator first, Iterator last) {
    auto res = scope(self);
    res->parent = self;

    Iterator it = first;
    for(const symbol& s : symbols) {
      if(it == last) throw std::runtime_error("not enough values");
      res->locals.emplace(s, *it++);
    }
    
    if(it != last) {
      throw std::runtime_error("too many values");
    }
    
    return res;
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
  environment& operator()(std::string s, const T& value) {
    locals.emplace(s, value);
    return *this;
  }
  
};




#endif
