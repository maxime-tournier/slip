#ifndef SLAP_EVAL_HPP
#define SLAP_EVAL_HPP

#include "symbol.hpp"
#include "environment.hpp"
#include "list.hpp"
#include "ast.hpp"

struct value;
using env = environment<value>;

// values
struct lambda {
  const list<symbol> args;
  const ref<ast::expr> body;
  const ref<env> scope;

  friend std::ostream& operator<<(std::ostream& out, const lambda& self);
};

struct value : variant<unit, real, integer, boolean, symbol, lambda, list<value> > {
  using value::variant::variant;
  using list = list<value>;
};

value apply(const value& func, const value::list& args);

value eval(const ref<env>& e, const ast::expr& expr);
value eval(const ref<env>& e, const ast::toplevel& expr);

#endif
