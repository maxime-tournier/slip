#ifndef SLAP_SEXPR_HPP
#define SLAP_SEXPR_HPP

#include <functional>

#include "variant.hpp"
#include "base.hpp"
#include "symbol.hpp"
#include "list.hpp"

#include "maybe.hpp"

// s-expressions
struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
  using list = list<sexpr>;

  static maybe<sexpr> parse(std::istream& in);

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  static void iter(std::istream& in, std::function<void(sexpr)> cont);
};

const extern char selection_prefix, injection_prefix;




#endif
