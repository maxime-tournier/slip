#include "ir.hpp"

#include "ast.hpp"
#include "tool.hpp"

#include <algorithm>


namespace ir {

  closure::closure(std::size_t argc, std::vector<expr> captures, expr body)
    : argc(argc),
      captures(captures),
      body(body) { }

  
  struct state {
    const state* parent;
    
    using locals_type = std::map<symbol, local>;
    locals_type locals;
    
    std::map<symbol, capture> captures;

    state& def(symbol name);
    
    expr find(symbol name);

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

    // push defs
    std::vector<expr> items;
    for(ast::bind def : self.defs) {
      items.emplace_back(make_ref<push>(compile(ctx, def.value)));
    }
    const std::size_t size = items.size();
    
    // compile let body
    items.emplace_back(compile(ctx, *self.body));
    
    return make_ref<scope>(size, seq{items});
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
    std::vector<expr> captures;
    for(auto cap : ordered) {
      captures.emplace_back(ctx->find(cap.first));
    }
    
    return make_ref<closure>(size(self.args), captures, body);
  }

  
  static expr compile(state* ctx, ast::app self) {
    const expr func = compile(ctx, *self.func);

    std::vector<expr> args;
    for(ast::expr arg : self.args) {
      args.emplace_back(compile(ctx, arg));
    };

    return call{make_ref<expr>(func), args};
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
  
}
