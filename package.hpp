#ifndef SLAP_PACKAGE_HPP
#define SLAP_PACKAGE_HPP

#include "symbol.hpp"
#include "type.hpp"
#include "eval.hpp"

// TODO rename? 
struct package {
  const symbol name;
  const ref<type::state> ts;
  eval::state::ref es;
  
  package& def(symbol name, type::mono t, eval::value v);

  package(symbol name);

  void exec(std::string filename);
  // void use(const package& other);
  
  // exec ast
  using cont_type = std::function<void(type::poly)>;
  void exec(ast::expr expr, cont_type cont=nullptr);
  
  type::poly sig() const;
  eval::value dict() const;

  static package import(symbol name);
  static std::string resolve(symbol name);
  
  static std::vector<std::string> path;
  static package builtins();

  // iterate imported packages
  const package* next = nullptr;
  static const package* first;
};

#endif
