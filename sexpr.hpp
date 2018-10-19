#ifndef SLAP_SEXPR_HPP
#define SLAP_SEXPR_HPP

#include "variant.hpp"
#include "base.hpp"
#include "symbol.hpp"
#include "list.hpp"


// s-expressions
struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
  using list = list<sexpr>;
};

sexpr parse(std::istream& in);



#endif
