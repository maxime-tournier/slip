#include "ir.hpp"

#include "ast.hpp"
#include "tool.hpp"

#include <algorithm>

#include "sexpr.hpp"

namespace ir {

  closure::closure(std::size_t argc, vector<expr> captures, expr body)
    : argc(argc),
      captures(captures),
      body(body) { }

  
  struct state {
    const state* parent = nullptr;
    
    using locals_type = std::map<symbol, local>;
    locals_type locals;
    
    std::map<symbol, capture> captures;

    state& def(symbol name);
    
    expr find(symbol name);

    // currently defined value, if any
    const symbol* self = nullptr;
    
    struct scope {
      state* owner;
      locals_type locals;

      scope(state* owner):
        owner(owner),
        locals(owner->locals) { }

      ~scope() {
        owner->locals = std::move(locals);
      }
    };
    
  };

  
  
  state& state::def(symbol name) {
    auto info = locals.emplace(name, locals.size());
    (void) info; assert(info.second);
    return *this;
  }
  
  
  expr state::find(symbol name) {
    // try locals first
    auto loc = locals.find(name);
    if(loc != locals.end()) return loc->second;

    // then try captures
    auto cap = captures.find(name);
    if(cap != captures.end()) return cap->second;

    // add capture
    if(parent) {
      return captures.emplace(name, captures.size()).first->second;
    } else {
      return global{name};
    }
  }



  ////////////////////////////////////////////////////////////////////////////////
  static expr compile(state* ctx, ast::expr self);
  
  template<class T>
  static expr compile(state* ctx, const T& self) {
    throw std::runtime_error("compile not implemented for: "
                             + tool::type_name(typeid(T)));
  };


  template<class T>
  static expr compile(state* ctx, ast::lit<T> self) {
    return lit<T>{self.value};
  }

  static expr compile(state* ctx, ast::var self) {
    return ctx->find(self.name);
  }

  
  static expr compile(state* ctx, ast::let self) {
    const state::scope backup(ctx);
    
    // allocate space for variables
    for(ast::bind def : self.defs) {
      ctx->def(def.id.name);
    }

    // push defined values
    vector<expr> items;
    for(ast::bind def : self.defs) {
      // TODO exception safety
      ctx->self = &def.id.name;
      ir::expr value = compile(ctx, def.value);
      ctx->self = nullptr;

      items.emplace_back(std::move(value));
    }
    const std::size_t locals = items.size();
    
    // compile let body
    items.emplace_back(compile(ctx, *self.body));
    items.emplace_back(exit{locals});
    
    return block{std::move(items)};
  }

  
  static expr compile(state* ctx, ast::abs self) {
    state sub = {ctx};

    // allocate stack for function arguments
    for(auto arg : self.args) {
      sub.def(arg.name());
    }

    // compile function body
    const expr body = compile(&sub, *self.body);

    // order captures by index
    std::vector<std::pair<symbol, capture>> ordered = {sub.captures.begin(),
                                                       sub.captures.end()};
    
    std::sort(ordered.begin(), ordered.end(), [](auto lhs, auto rhs) {
        return lhs.second.index < rhs.second.index;
      });
    
    // define function captures
    vector<expr> captures;
    for(auto cap : ordered) {
      captures.emplace_back(ctx->find(cap.first));
    }
    
    return make_ref<closure>(size(self.args), captures, body);
  }

  
  static expr compile(state* ctx, ast::app self) {
    return self.func->match([&](const ast::expr& func) -> expr {
        vector<expr> items;

        // push func
        items.emplace_back(compile(ctx, func));

        // push args
        std::size_t argc = 0;
        for(ast::expr arg : self.args) {
          items.emplace_back(compile(ctx, arg));
          ++argc;
        };

        // call
        items.emplace_back(call{argc});

        return block{std::move(items)};
      },
      
      [&](const ast::sel& func) -> expr {
        assert(size(self.args) == 1);
        vector<expr> items;
        items.emplace_back(compile(ctx, self.args->head));
        items.emplace_back(sel{func.id.name});

        return block{std::move(items)};
      });
  }


  static expr compile(state* ctx, ast::cond self) {
    vector<expr> items;
    items.emplace_back(compile(ctx, *self.test));
    items.emplace_back(make_ref<cond>(compile(ctx, *self.conseq),
                                      compile(ctx, *self.alt)));
    return block{items};
  }
  

  static expr compile(state* ctx, ast::import self) {
    // TODO differentiate between topleve/local imports
    if(ctx->parent) {    
      throw std::runtime_error("unimplemented: non-toplevel import");
    } else {
      vector<expr> items;
      items.emplace_back(import{self.package});
      items.emplace_back(def{self.package});
      return block{items};
    }
  }


  static expr compile(state* ctx, ast::def self) {
    if(ctx->parent) {
      throw std::runtime_error("unimplemented: non-toplevel def");
    } else {
      vector<expr> items;
      items.emplace_back(compile(ctx, *self.value));
      items.emplace_back(def{self.id.name});
      return block{items};
    }
  }


  
  static expr compile(state* ctx, ast::use self) {
    if(ctx->parent) {
      throw std::runtime_error("unimplemented: non-toplevel use");
      // TODO get help from the type system?
    } else {
      return make_ref<use>(compile(ctx, *self.env));
    }
  }

  
  ////////////////////////////////////////////////////////////////////////////////  
  static expr compile(state* ctx, ast::expr self) {
    return self.match([&](auto self) {
        return compile(ctx, self);
      });
  };

  

  expr compile(const ast::expr& self) {
    state ctx;
    return compile(&ctx, self);
  }


  struct repr_visitor {
    template<class T>
    sexpr operator()(const T& self) const {
      string res;
      res += "#<" + tool::type_name(typeid(T)) + ">";
      return res;
    }

    template<class T>
    sexpr operator()(const lit<T>& self) const { return self.value; }

    sexpr operator()(const lit<unit>& self) const { return sexpr::list(); }

    sexpr operator()(const drop& self) const {
      return symbol("drop")
        >>= sexpr::list();
    }
    
    sexpr operator()(const block& self) const {
      return symbol("block")
        >>= foldr(sexpr::list(), self.items, [&](sexpr::list rhs, ir::expr lhs) {
          return repr(lhs) >>= rhs;
        });
    }

    sexpr operator()(const exit& self) const {
      return symbol("exit")
        >>= integer(self.locals)
        >>= sexpr::list();
    }

    sexpr operator()(const ref<closure>& self) const {
      return symbol("closure")
        >>= integer(self->argc)
        >>= foldr(sexpr::list(), self->captures,
                  [&](sexpr::list tail, ir::expr head) {
                    return repr(head) >>= tail;
                  })
        >>= repr(self->body)
        >>= sexpr::list();
    }

    sexpr operator()(const global& self) const {
      return symbol("glob")
        >>= self.name
        >>= sexpr::list();
    }

    sexpr operator()(const local& self) const {
      return symbol("var")
        >>= integer(self.index)
        >>= sexpr::list();
    }

    sexpr operator()(const capture& self) const {
      return symbol("cap")
        >>= integer(self.index)
        >>= sexpr::list();
    }

    sexpr operator()(const ref<cond>& self) const {
      return symbol("cond")
        >>= repr(self->then)
        >>= repr(self->alt)
        >>= sexpr::list();
    }

    sexpr operator()(const call& self) const {
      return symbol("call")
        >>= integer(self.argc)
        >>= sexpr::list();
    }

    sexpr operator()(const def& self) const {
      return symbol("def")
        >>= self.name
        >>= sexpr::list();
    }


    sexpr operator()(const ref<use>& self) const {
      return symbol("use")
        >>= repr(self->env)
        >>= sexpr::list();
    }
    

    
    sexpr operator()(const import& self) const {
      return symbol("import")
        >>= self.package
        >>= sexpr::list();
    }

    sexpr operator()(const sel& self) const {
      return symbol("sel")
        >>= self.attr
        >>= sexpr::list();
    }
    
  };
  
  sexpr repr(const ir::expr& self) {
    return self.match(repr_visitor());
  }

  
}
