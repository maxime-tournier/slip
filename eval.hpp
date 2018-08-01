#ifndef SLAP_EVAL_HPP
#define SLAP_EVAL_HPP

#include <functional>

#include "symbol.hpp"
#include "environment.hpp"
#include "list.hpp"
#include "ast.hpp"

struct value;
using env = environment<value>;

struct lambda {
  const list<symbol> args;
  const ref<ast::expr> body;
  const ref<env> scope;
};

using builtin = std::function< value(const list<value>&) >;

struct value : variant<unit, real, integer, boolean, symbol, list<value>,
                       lambda, builtin > {
  using value::variant::variant;
  using list = list<value>;

  friend std::ostream& operator<<(std::ostream& out, const value& self);
};

value apply(const value& func, const value::list& args);

value eval(const ref<env>& e, const ast::expr& expr);
value eval(const ref<env>& e, const ast::toplevel& expr);

#endif
