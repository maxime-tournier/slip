#ifndef SLAP_EVAL_HPP
#define SLAP_EVAL_HPP

#include <functional>
#include <vector>

#include "symbol.hpp"
#include "environment.hpp"
#include "list.hpp"
#include "ast.hpp"

namespace eval {

  struct value;

  
  struct state : environment<value> {
    using state::environment::environment;
  };

  
  struct record {
    std::map<symbol, value> attrs;
  };

  
  struct sum {
    symbol tag;
    ref<value> data;
    sum(symbol tag, const value& self);
  };
  
  
  struct closure {
    using func_type = std::function<value(const value* args)>;
    const func_type func;
  
    const std::size_t argc;

    closure(std::size_t argc, func_type func);

    closure(closure&& other);
    closure(const closure&);
  
    template<class Ret, class ... Args>
    closure(Ret (*impl) (const Args&...) );
  };

  
  struct value : variant<unit, real, integer, boolean, symbol, list<value>,
                         closure, record, sum> {
    using value::variant::variant;
    using list = list<value>;

    friend std::ostream& operator<<(std::ostream& out, const value& self);
  };

  const extern symbol cons, nil, head, tail;
  
  template<class Ret, class ... Args>
  closure::closure(Ret (*impl) (const Args&...) )
    : func([impl](const value* args) -> value {
        const std::tuple<const Args&...> pack = {
          *(args++)->template get<Args>()...
        };
        return tool::apply(impl, pack, std::index_sequence_for<Args...>());
      }),
      argc(sizeof...(Args)) { }


  value apply(const value& func, const value* first, const value* last);
  value eval(const ref<state>& e, const ast::expr& expr);
  
}


#endif
