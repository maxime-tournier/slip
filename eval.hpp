#ifndef SLAP_EVAL_HPP
#define SLAP_EVAL_HPP

#include <functional>
#include <vector>

#include "symbol.hpp"
#include "environment.hpp"
#include "list.hpp"
#include "ast.hpp"

#include "gc.hpp"

namespace eval {

  struct value;

  
  struct state : environment<value> {
    using state::environment::environment;
  };

  
  struct record : gc {
    std::map<symbol, value> attrs;
  };

  
  struct sum;
  
  struct builtin {
    using func_type = std::function<value(const value* args)>;
    func_type func;
  
    std::size_t argc;

    builtin(std::size_t argc, func_type func);

    template<class Ret, class ... Args>
    builtin(Ret (*impl) (const Args&...));
  };
  

  struct closure : gc {
    closure(ref<state> env, std::vector<symbol> args, ast::expr body);
    ref<state> env;
    std::vector<symbol> args;
    ast::expr body;
  };

  
  struct module {
    enum type {
      product,
      coproduct,
      list,
    } type;
  };

  
  struct value : variant<unit, real, integer, boolean, symbol, list<value>,
                         builtin,
                         closure*, record*, sum*,
                         module> {
    using value::variant::variant;
    using list = list<value>;

    friend std::ostream& operator<<(std::ostream& out, const value& self);
  };


  struct sum : gc {
    sum(symbol tag, const value& data);
    
    symbol tag;
    value data;
  };

  
  const extern symbol cons, nil, head, tail;

  
  template<class Ret, class ... Args>
  builtin::builtin(Ret (*impl) (const Args&...) )
    : func([impl](const value* args) -> value {
        const std::tuple<const Args&...> pack = {
          *(args++)->template get<Args>()...
        };
        return tool::apply(impl, pack, std::index_sequence_for<Args...>());
      }),
      argc(sizeof...(Args)) { }


  value apply(const value& func, const value* first, const value* last);
  value eval(const ref<state>& e, const ast::expr& expr);

  
  void collect(const ref<state>& e, bool debug=false);
  
}


#endif
