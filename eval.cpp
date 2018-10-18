#include "eval.hpp"

#include <vector>

#include "sexpr.hpp"
#include "package.hpp"

namespace eval {

  closure::closure(std::size_t argc, func_type func)
    : func(func),
      argc(argc) {

  }

  closure::closure(const closure&) = default;
  closure::closure(closure&&) = default;

  sum::sum(symbol tag, const value& data)
    : tag(tag), data(make_ref<value>(data)) { }
  
  // apply a closure to argument range
  static value apply(const closure& self, const value* first, const value* last) {
    const std::size_t argc = last - first;

    if(argc < self.argc) {
      // unsaturated call: build wrapper
      const std::size_t remaining = self.argc - argc;        
      const std::vector<value> saved(first, last);
    
      return closure(remaining, [self, saved, remaining](const value* args) {
          std::vector<value> tmp = saved;
          for(auto it = args, last = args + remaining; it != last; ++it) {
            tmp.emplace_back(*it);
          }
          return apply(self, tmp.data(), tmp.data() + tmp.size());
        });
    }

    if(argc > self.argc) {
      // over-saturated call: call result with remaining args
      const value* mid = first + argc;
      
      const value func = apply(self, first, mid);
      assert(func.get<closure>() && "type error");
      
      const closure& self = func.cast<closure>();
      return apply(self, mid, last);
    }

    // saturated calls
    return self.func(first);
  }

  
value apply(const value& self, const value* first, const value* last) {
  return self.match([&](const closure& self) { return apply(self, first, last); },
                    [&](const value& self) -> value {
                      throw std::runtime_error("type error in application");
                    });
}


  template<class T>
  static value eval(const ref<state>&, const T& ) {
    throw std::logic_error("eval unimplemented: " + tool::type_name(typeid(T)));
  }

  
  template<class T>
  static value eval(const ref<state>&, const ast::lit<T>& self) {
    return self.value;
  }


  static value eval(const ref<state>& e,const ast::var& self) {
    return e->find(self.name);
  }

  
  static value eval(const ref<state>& e, const ast::app& self) {
    const value func = eval(e, *self.func);
    assert(func.get<closure>() && "type error");
    
    const closure& clos = func.cast<closure>();
    
    std::vector<value> args; args.reserve(clos.argc);
    foldl(unit(), self.args, [&](unit, const ast::expr& self) {
        args.emplace_back(eval(e, self));
        return unit();
      });
    
    return apply(clos, args.data(), args.data() + args.size());
  }

  
  static value eval(const ref<state>& e, const ast::abs& self) {
    const list<symbol> args = map(self.args, [](const ast::abs::arg& arg) {
        return arg.name();
      });

    const std::size_t argc = size(args);
    
    const ast::expr body = *self.body;

    // note: weak ref to break cycles
    const std::weak_ptr<state> scope = e;
    
    return closure(argc, [argc, args, scope, body](const value* values) -> value {
      return eval(augment(scope.lock(), args, values, values + argc), body);
    });
  }

  
  static value eval(const ref<state>& e, const ast::io& self) {
    return self.match([&](const ast::expr& self) {
        return eval(e, self);
      },
      [&](const ast::bind& self) {
        return eval(e, self);
      });
  }

  

  static value eval(const ref<state>& e, const ast::seq& self) {
    const value init = unit();
    return foldl(init, self.items, [&](const value&, const ast::io& self) {
        return eval(e, self);
      });
  }


  static value eval(const ref<state>& e, const ast::module& self) {
    // just define the reified module type constructor
    auto it = e->locals.emplace(self.id.name, unit()); (void) it;
    assert(it.second && "redefined variable");    
    return unit();
  }

  static value eval(const ref<state>& e, const ast::def& self) {
    auto it = e->locals.emplace(self.id.name, eval(e, *self.value)); (void) it;
    assert(it.second && "redefined variable");
    return unit();
  }
  
  
  static value eval(const ref<state>& e, const ast::let& self) {
    auto sub = scope(e);
    state::locals_type locals;
    
    for(const ast::bind& def : self.defs) {
      locals.emplace(def.id.name, eval(sub, def.value));
    }

    sub->locals = std::move(locals);
    return eval(sub, *self.body);
  }

  
  static value eval(const ref<state>& e, const ast::cond& self) {
    const value test = eval(e, *self.test);
    assert(test.get<boolean>() && "type error");
    
    if(test.cast<boolean>()) return eval(e, *self.conseq);
    else return eval(e, *self.alt);
  }

				   
  

  // static value eval(const ref<state>& e, const ast::expr& self) {
  //   return self.visit(expr_visitor(), e);
  // }

  static value eval(const ref<state>& e, const ast::record& self) {
    record res;
    for(const auto& attr : self.attrs) {
      res.attrs.emplace(attr.id.name, eval(e, attr.value));
    }
    return res;
  }


  static value eval(const ref<state>& e, const ast::sel& self) {
    const symbol name = self.id.name;
    return closure(1, [name](const value* args) -> value {
        const auto it = args[0].cast<record>().attrs.find(name); (void) it;
        assert(it != args[0].cast<record>().attrs.end() && "type error");
        return it->second;
      });
  }


  static value eval(const ref<state>& e, const ast::inj& self) {
    const symbol tag = self.id.name;
    return closure(1, [tag](const value* args) -> value {
        return sum(tag, args[0]);
      });
  }
  

  static value eval(const ref<state>& e, const ast::make& self) {
    return eval(e, ast::record{self.attrs});
  }

  
  static value eval(const ref<state>& e, const ast::use& self) {
    const value env = eval(e, *self.env);
    assert(env.get<record>() && "type error");

    auto s = scope(e);
    
    for(const auto& it : env.cast<record>().attrs) {
      s->locals.emplace(it.first, it.second);
    }

    return eval(s, *self.body);
  }
  

  static value eval(const ref<state>& e, const ast::import& self) {
    const package pkg = package::import(self.package);
    e->def(self.package, pkg.dict());
    return unit();
  }



  static value eval(const ref<state>& e, const ast::match& self) {
    std::map<symbol, std::pair<symbol, ast::expr>> dispatch;

    for(const auto& handler : self.cases) {
      auto err = dispatch.emplace(handler.id.name,
                                  std::make_pair(handler.arg.name(),
                                                 handler.value));
      (void) err; assert(err.second);
    }
    
    const ref<ast::expr> fallback = self.fallback;
    
    return closure(1, [e, dispatch, fallback](const value* args) {
        const auto& self = args[0].cast<sum>();
        auto it = dispatch.find(self.tag);
        if(it != dispatch.end()) {
          auto sub = scope(e);
          sub->def(it->second.first, *self.data);
          return eval(sub, it->second.second);
        } else {
          assert(fallback);
          return eval(e, *fallback);
        }
      });
  }

  namespace {
    struct eval_visitor {
  
      template<class T>
      value operator()(const T& self, const ref<state>& e) const {
        return eval(e, self);
      }
  
    };
  }

  value eval(const ref<state>& e, const ast::expr& self) {
    return self.visit(eval_visitor(), e);
  }

  
namespace {
  
struct ostream_visitor {
  using type = void;
  
  template<class T>
  void operator()(const T& self, std::ostream& out) const {
	out << self;
  }

  void operator()(const closure& self, std::ostream& out) const {
	out << "#<fun>";
  }
  
  void operator()(const unit& self, std::ostream& out) const {
	out << "()";
  }

  void operator()(const boolean& self, std::ostream& out) const {
	out << (self ? "true" : "false");
  }

  void operator()(const integer& self, std::ostream& out) const {
    out << self; //  << "i";
  }

  void operator()(const real& self, std::ostream& out) const {
    out << self; // << "d";
  }

  void operator()(const record& self, std::ostream& out) const {
    out << "{";
    bool first = true;
    for(const auto& it : self.attrs) {
      if(first) first = false;
      else out << "; ";
      out << it.first << ": " << it.second;
    }
    out << "}";
  }

  void operator()(const sum& self, std::ostream& out) const {
    out << "<" << self.tag << ": " << *self.data << ">";
  }

  
};
}

std::ostream& operator<<(std::ostream& out, const value& self) {
  self.visit(ostream_visitor(), out);
  return out;
}

}
