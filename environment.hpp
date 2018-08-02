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

  template<class Iterator>
  friend ref<environment> augment(const ref<environment>& self,
                                  const list<symbol>& args,
                                  Iterator first, Iterator last) {
    auto res = std::make_shared<environment>();
    res->parent = self;
    if(last != foldl(first, args, [&](Iterator it, const symbol& name) {
          if(it == last) throw std::runtime_error("not enough values");
          res->locals.emplace(name, *it++);
          return it;
        })) {
      throw std::runtime_error("too many values");
    }
    
    return res;
  };

  T& find(const symbol& s) {
    auto it = locals.find(s);
    if(it != locals.end()) return it->second;
    
    if(!parent) {
      throw std::out_of_range("unbound variable " + tool::quote(s.get()));
    }
    
    return parent->find(s);
  }

  template<class ... Args>
  environment& operator()(symbol s, Args&& ... args) {
    locals.emplace(s, std::forward<Args>(args)...);
    return *this;
  }
  
};




#endif
