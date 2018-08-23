#include "eval.hpp"

#include <vector>

#include "sexpr.hpp"

closure::closure(std::size_t argc, func_type func)
  : func(func),
    argc(argc) {

}

closure::closure(const closure&) = default;
closure::closure(closure&&) = default;

static value eval(const ref<env>& e, const ast::io& self);

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


struct eval_visitor {
  using type = value;
  
  template<class T>
  T operator()(const ast::lit<T>& self, const ref<env>& ) const {
	return self.value;
  }

  
  value operator()(const ast::var& self, const ref<env>& e) const {
	try {
	  return e->find(self.name);
	} catch(std::out_of_range& e) {
	  throw std::runtime_error(e.what());
	}
  }

  
  value operator()(const ast::app& self, const ref<env>& e) const {
	const value func = eval(e, *self.func);

    std::vector<value> args;
    foldl(unit(), self.args, [&](unit, const ast::expr& self) {
        args.emplace_back(eval(e, self));
        return unit();
      });
    
    return apply(func, args.data(), args.data() + args.size());
  }
  
  value operator()(const ast::abs& self, const ref<env>& e) const {
    const list<symbol> args = map(self.args, [](const ast::abs::arg& arg) {
        return arg.name();
      });
    
    const std::size_t argc = size(args);
    
    const ast::expr body = *self.body;
    const ref<env> scope = e;
    
    return closure(argc, [argc, args, scope, body](const value* values) -> value {
      return eval(augment(scope, args, values, values + argc), body);
    });
  }

  
  value operator()(const ast::seq& self, const ref<env>& e) const {
	const value init = unit();
	return foldl(init, self.items,
				 [&](const value&, const ast::io& self) -> value {
				   return eval(e, self);
				 });
  }


  value operator()(const ast::def& self, const ref<env>& e) const {
	// note: the type system will keep us from evaluating side effects if the
	// variable is already defined
	auto res = e->locals.emplace(self.name, eval(e, self.value));
	if(!res.second) {
	  throw std::runtime_error("redefined variable " + tool::quote(self.name.get()));
	}
							   
	return unit();
  }

  
  value operator()(const ast::let& self, const ref<env>& e) const {
    auto sub = scope(e);
    env::locals_type locals;
    
    for(const ast::def& def : self.defs) {
      locals.emplace(def.name, eval(sub, def.value));
    }

    sub->locals = std::move(locals);
    return eval(sub, *self.body);
  }

  
  value operator()(const ast::cond& self, const ref<env>& e) const {
    const value test = eval(e, *self.test);
    if(auto b = test.get<boolean>()) {
      if(*b) return eval(e, *self.conseq);
      else return eval(e, *self.alt);
    } else throw std::runtime_error("condition must be boolean");
  }

				   
  value operator()(const ast::io& self, const ref<env>& e) const {
	return self.visit(eval_visitor(), e);
  }

  value operator()(const ast::expr& self, const ref<env>& e) const {
	return self.visit(eval_visitor(), e);
  }

  value operator()(const ast::rec& self, const ref<env>& e) const {
    record res;
    foldl(unit(), self.attrs, [&](unit, const ast::rec::attr& attr) {
      res.attrs.emplace(attr.name, eval(e, attr.value));
      return unit();
    });
    return res;
  }


  value operator()(const ast::sel& self, const ref<env>& e) const {
    const symbol name = self.name;
    return closure(1, [name](const value* args) -> value {
      return args[0].cast<record>().attrs.at(name);      
      // return args[0].cast<record>().attrs.find(name)->second;
    });
  }


  value operator()(const ast::make& self, const ref<env>& e) const {
    return eval(e, ast::rec{self.attrs});
  }
  
  
  template<class T>
  value operator()(const T& self, const ref<env>& e) const {
	throw std::logic_error("eval unimplemented: " + tool::type_name(typeid(T)));
  }
  
};


static value eval(const ref<env>& e, const ast::io& self) {
  return self.visit(eval_visitor(), e);
}

value eval(const ref<env>& e, const ast::expr& self) {
  return self.visit(eval_visitor(), e);
}

  
value eval(const ref<env>& e, const ast::toplevel& self) {
  return self.visit(eval_visitor(), e);
}


namespace {
struct ostream {
  using type = void;
  
  template<class T>
  void operator()(const T& self, std::ostream& out) const {
	out << self;
  }

  void operator()(const closure& self, std::ostream& out) const {
	out << "#<closure>";
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
  self.visit(ostream(), out);
  return out;
}
