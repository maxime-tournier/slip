#ifndef SLAP_PACKAGE_HPP
#define SLAP_PACKAGE_HPP

#include "symbol.hpp"

#include <map>
#include <vector>
#include <functional>

namespace ast {
  struct expr;
}

namespace package {

  std::string resolve(symbol name);
  std::vector<std::string>& path();

  template<class Value>
  const Value& import(symbol name, std::function<Value()> cont) {
    static std::map<symbol, Value> cache;
    auto it = cache.find(name);
    if(it != cache.end()) return it->second;
    
    return cache.emplace(name, cont()).first->second;
  }

  
  // convenience: iterate ast
  void iter(symbol name, std::function<void(ast::expr)> func);
}


#endif
