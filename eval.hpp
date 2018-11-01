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

  struct tag;
  using gc = class gc<tag>;

  struct state {
    using ref = gc::ref<state>;
    
    ref parent;
    std::map<symbol, value> locals;

    value* find(symbol name);
    friend ref scope(ref self);
    
    state(ref parent={});

    void def(symbol name, const value&);
  };
  
  using record = std::map<symbol, value>;
  
  struct sum;
  
  struct closure {
    using func_type = std::function<value(const value* args)>;
    func_type func;
  
    std::size_t argc;

    closure(std::size_t argc, func_type func);

    template<class Ret, class ... Args>
    closure(Ret (*impl) (const Args&...));
  };
  

  // note: we need separate lambdas because we need to traverse env during gc
  struct lambda : closure {
    state::ref env;
    lambda(state::ref env, std::size_t argc, func_type func);
  };

  
  struct module {
    enum type {
      product,
      coproduct,
      list,
    } type;
  };

  
  struct value : variant<unit, real, integer, boolean, symbol,
                         ref<string>, list<value>,
                         closure,
                         lambda, ref<record>, ref<sum>,
                         module,
                         ref<value>> {
    using value::variant::variant;
    using list = list<value>;

    friend std::ostream& operator<<(std::ostream& out, const value& self);
  };


  struct sum : value {
    sum(const value& data, symbol tag);
    const symbol tag;
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
  value eval(state::ref e, const ast::expr& expr);

  
  void mark(state::ref e, bool debug=false);
  
}


#endif
