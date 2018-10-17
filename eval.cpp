#include "eval.hpp"

#include <vector>

#include "sexpr.hpp"
#include "package.hpp"

namespace eval {
// env::~env() {
//   std::clog << __func__ << std::endl;
// }

closure::closure(std::size_t argc, func_type func)
  : func(func),
    argc(argc) {

}

closure::closure(const closure&) = default;
closure::closure(closure&&) = default;

value apply(const value& self, const value* first, const value* last) {
  const std::size_t argc = last - first;
  const std::size_t expected = self.match<std::size_t>
    ([](const closure& self) { return self.argc; },
     [](const value& self) -> std::size_t {
       throw std::runtime_error("type error in application");
     });

  if(argc < expected) {
    // unsaturated call: build wrapper
    const std::size_t remaining = expected - argc;        
    const std::vector<value> saved(first, last);
    
    return closure(remaining, [self, saved, remaining](const value* args) {
      std::vector<value> tmp = saved;
      for(auto it = args, last = args + remaining; it != last; ++it) {
        tmp.emplace_back(*it);
      }
      return apply(self, tmp.data(), tmp.data() + tmp.size());
    });
  }

  if(argc > expected) {
    // over-saturated call: call result with remaining args
    const value* mid = first + expected;
    const value result = apply(self, first, mid);
    return apply(result, mid, last);
  }

  // saturated calls
  return self.match<value>
	([first](const closure& self) {
      return self.func(first);      
    },
	 [](value) -> value {
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
    try {
      return e->find(self.name);
    } catch(std::out_of_range& e) {
      throw std::runtime_error(e.what());
    }
  }

  
  static value eval(const ref<state>& e, const ast::app& self) {
    const value func = expr(e, *self.func);

    std::vector<value> args;
    foldl(unit(), self.args, [&](unit, const ast::expr& self) {
        args.emplace_back(expr(e, self));
        return unit();
      });
    
    return apply(func, args.data(), args.data() + args.size());
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
      return expr(augment(scope.lock(), args, values, values + argc), body);
    });
  }

  static value eval(const ref<state>& e, const ast::io& self) {
    return self.match<value>([&](const ast::expr& self) {
        return expr(e, self);
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
    auto it = e->locals.emplace(self.name, unit()); (void) it;
    assert(it.second && "redefined variable");    
    return unit();
  }

  static value eval(const ref<state>& e, const ast::def& self) {
    auto it = e->locals.emplace(self.name, expr(e, *self.value)); (void) it;
    assert(it.second && "redefined variable");
    return unit();
  }
  
  
  static value eval(const ref<state>& e, const ast::let& self) {
    auto sub = scope(e);
    state::locals_type locals;
    
    for(const ast::bind& def : self.defs) {
      locals.emplace(def.name, expr(sub, def.value));
    }

    sub->locals = std::move(locals);
    return expr(sub, *self.body);
  }

  
  static value eval(const ref<state>& e, const ast::cond& self) {
    const value test = expr(e, *self.test);
    assert(test.get<boolean>() && "type error");
    
    if(test.cast<boolean>()) return expr(e, *self.conseq);
    else return expr(e, *self.alt);
  }

				   
  

  // static value eval(const ref<state>& e, const ast::expr& self) {
  //   return self.visit(expr_visitor(), e);
  // }

  static value eval(const ref<state>& e, const ast::record& self) {
    return foldl(unit(), self.attrs, [&](unit, const ast::record::attr& attr) {
        res.attrs.emplace(attr.name, expr(e, attr.value));
        return unit();
      });
  }


  static value eval(const ref<state>& e, const ast::sel& self) {
    const symbol name = self.name;
    return closure(1, [name](const value* args) -> value {
      return args[0].cast<record>().attrs.at(name);      
      // return args[0].cast<record>().attrs.find(name)->second;
    });
  }


  static value eval(const ref<state>& e, const ast::make& self) {
    return expr(e, ast::record{self.attrs});
  }

  
  static value eval(const ref<state>& e, const ast::use& self) {
    auto r = expr(e, *self.env);
    assert(r.get<record>() && "type error");

    auto s = scope(e);
    
    for(const auto& it : rr.cast<record>().attrs) {
      s->locals.emplace(it.first, it.second);
    }

    return expr(s, *self.body);
  }
  

  static value eval(const ref<state>& e, const ast::import& self) {
    const package pkg = package::import(self.package);
    e->def(self.package, pkg.dict());
    return unit();
  }
  
  
  struct expr_visitor {
    using type = value;
  
    template<class T>
    value operator()(const T& self, const ref<state>& e) const {
      return eval(e, self);
    }
  
  };


  value expr(const ref<state>& e, const ast::expr& self) {
    return self.visit(expr_visitor(), e);
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

};
}

std::ostream& operator<<(std::ostream& out, const value& self) {
  self.visit(ostream_visitor(), out);
  return out;
}

}
