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
    throw std::runtime_error("compile not implemented for: " + tool::type_name(typeid(T)));
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
    vector<expr> defs;
    for(ast::bind def : self.defs) {
      // TODO exception safety
      ctx->self = &def.id.name;
      ir::expr value = compile(ctx, def.value);
      ctx->self = nullptr;

      defs.emplace_back(std::move(value));
    }
    
    // compile let body
    expr value = compile(ctx, *self.body);
    
    return make_ref<scope>(defs, value);
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
    const expr func = compile(ctx, *self.func);

    vector<expr> args;
    for(ast::expr arg : self.args) {
      args.emplace_back(compile(ctx, arg));
    };

    return make_ref<call>(func, args);
  }


  static expr compile(state* ctx, ast::cond self) {
    return make_ref<cond>(compile(ctx, *self.test),
                          compile(ctx, *self.conseq),
                          compile(ctx, *self.alt));
  }
  

  static expr compile(state* ctx, ast::import self) {
    return import{self.package};
  }

  static expr compile(state* ctx, ast::use self) {
    return make_ref<use>(compile(ctx, *self.env));
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

    sexpr operator()(const seq& self) const {
      return symbol("seq")
        >>= foldr(sexpr::list(), self.items, [&](sexpr::list rhs, ir::expr lhs) {
          return repr(lhs) >>= rhs;
        });
    }

    sexpr operator()(const ref<scope>& self) const {
      return symbol("scope")
        >>= foldr(sexpr::list(), self->defs, [&](sexpr::list rhs, ir::expr lhs) {
            return repr(lhs) >>= rhs;
          })
        >>= repr(self->value)
        >>= sexpr::list();
    }

    sexpr operator()(const ref<closure>& self) const {
      return symbol("closure")
        >>= integer(self->argc)
        >>= foldr(sexpr::list(), self->captures, [&](sexpr::list tail, ir::expr head) {
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
      return symbol("if")
        >>= repr(self->test)
        >>= repr(self->conseq)
        >>= repr(self->alt)
        >>= sexpr::list();
    }

    sexpr operator()(const ref<call>& self) const {
      return repr(self->func)
        >>= foldr(sexpr::list(), self->args, [](sexpr::list tail, ir::expr head) {
            return repr(head) >>= tail;
          });
    }
    
  };
  
  sexpr repr(const ir::expr& self) {
    return self.match(repr_visitor());
  }

  
}
