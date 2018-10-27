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
  
  struct state : gc {
    state* parent;
    std::map<symbol, value> locals;

    value* find(symbol name);
    friend state* scope(state* self);
    state(state* parent=nullptr);

    void def(symbol name, const value&);
  };
  
  using record = std::map<symbol, value>;
  
  struct sum;
  
  struct builtin {
    using func_type = std::function<value(const value* args)>;
    func_type func;
  
    std::size_t argc;

    builtin(std::size_t argc, func_type func);

    template<class Ret, class ... Args>
    builtin(Ret (*impl) (const Args&...));
  };
  

  struct closure {
    closure(state* env, std::vector<symbol> args, ast::expr body);
    state* env;                 // TODO const?
    const std::vector<symbol> args;
    const ast::expr body;
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
                         builtin,
                         ref<closure>, ref<record>, ref<sum>,
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
  builtin::builtin(Ret (*impl) (const Args&...) )
    : func([impl](const value* args) -> value {
        const std::tuple<const Args&...> pack = {
          *(args++)->template get<Args>()...
        };
        return tool::apply(impl, pack, std::index_sequence_for<Args...>());
      }),
      argc(sizeof...(Args)) { }


  value apply(const value& func, const value* first, const value* last);
  value eval(state* e, const ast::expr& expr);

  
  void mark(state* e, bool debug=false);
  
}


#endif
