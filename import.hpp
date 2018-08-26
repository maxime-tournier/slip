#ifndef SLAP_IMPORT_HPP
#define SLAP_IMPORT_HPP

#include "symbol.hpp"

#include "type.hpp"
#include "ast.hpp"
#include "eval.hpp"

namespace import {
  void resolve(symbol name, std::function<void(std::istream&)> cont);

  type::mono typecheck(symbol name);
  eval::value import(symbol name);
  
  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };
}

#endif
