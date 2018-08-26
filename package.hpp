#ifndef SLAP_PACKAGE_HPP
#define SLAP_PACKAGE_HPP

#include "symbol.hpp"
#include "type.hpp"
#include "eval.hpp"

// TODO rename? 
struct package {
  const ref<type::state> ts;
  const ref<eval::state> es;
  
  package& def(symbol name, type::mono t, eval::value v);

  using resolver_type = std::function<std::string(symbol)>;
  package(resolver_type resolver);

  void exec(std::string filename);

  using cont_type = std::function<void(type::poly, eval::value)>;
  void exec(ast::expr expr, cont_type cont=nullptr);
  
  type::poly sig() const;
  eval::value dict() const;
};

#endif
