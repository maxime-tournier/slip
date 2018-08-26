#ifndef CORE_HPP
#define CORE_HPP

#include "type.hpp"
#include "eval.hpp"

namespace core {

  // setup the core package in type/eval states
  void setup(ref<type::state> s, ref<eval::state> e);
  
}

#endif
