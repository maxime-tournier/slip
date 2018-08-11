#ifndef SLAP_EVAL_HPP
#define SLAP_EVAL_HPP

#include <functional>
#include <vector>

#include "symbol.hpp"
#include "environment.hpp"
#include "list.hpp"
#include "ast.hpp"

struct value;
using env = environment<value>;

struct lambda {
  const std::vector<symbol> args;
  const ref<ast::expr> body;
  const ref<env> scope;
};

struct record {
  std::map<symbol, value> attrs;
};

struct builtin {
  using func_type = std::function<value (const value* args)>;
  const func_type func;
  
  const std::size_t argc;

  builtin(std::size_t argc, func_type func);

  builtin(builtin&& other);
  builtin(const builtin&);
  
  template<class Ret, class ... Args>
  builtin(Ret (*impl) (const Args&...) );
};

struct value : variant<unit, real, integer, boolean, symbol, list<value>,
                       lambda, builtin,
                       record> {
  using value::variant::variant;
  using list = list<value>;

  friend std::ostream& operator<<(std::ostream& out, const value& self);
};

template<class Ret, class ... Args>
builtin::builtin(Ret (*impl) (const Args&...) )
  : func( [impl](const value* args) -> value {
            const std::tuple<const Args&...> pack =
              { *(args++)->template get<Args>()... };
            return tool::apply(impl, pack, std::index_sequence_for<Args...>());
          }),
    argc(sizeof...(Args)) { }



value apply(const value& func, const value* first, const value* last);

value eval(const ref<env>& e, const ast::expr& expr);
value eval(const ref<env>& e, const ast::toplevel& expr);

#endif
