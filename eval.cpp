#include "eval.hpp"

#include <vector>

#include "sexpr.hpp"

builtin::builtin(std::size_t argc, func_type func)
  : func(func),
    argc(argc) {

}

builtin::builtin(const builtin&) = default;
builtin::builtin(builtin&&) = default;

static value eval(const ref<env>& e, const ast::io& self);

value apply(const value& func, const value* first, const value* last) {
  return func.match<value>
	([&](const lambda& self) {
      return eval(augment(self.scope, self.args, first, last), *self.body);
    },
	  [&](const builtin& self) {
		return self.func(first);
	  },
	  [&](value) -> value {
		throw std::runtime_error("type error in application");
	  } );
}


struct eval_visitor {
  
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
    std::vector<symbol> args;
    for(symbol s : self.args) {
      args.push_back(s);
    }
    
	return lambda{args, self.body, e};
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

  
  value operator()(const ast::cond& self, const ref<env>& e) const {
    const value test = eval(e, *self.test);
    if(auto b = test.get<boolean>()) {
      if(*b) return eval(e, *self.conseq);
      else return eval(e, *self.alt);
    } else throw std::runtime_error("condition must be boolean");
  }

				   
  value operator()(const ast::io& self, const ref<env>& e) const {
	return self.visit<value>(eval_visitor(), e);
  }

  value operator()(const ast::expr& self, const ref<env>& e) const {
	return self.visit<value>(eval_visitor(), e);
  }

  value operator()(const ast::record& self, const ref<env>& e) const {
    record res;
    foldl(unit(), self.attrs, [&](unit, const ast::record::attribute& attr) {
      res.attrs.emplace(attr.name, eval(e, attr.value));
      return unit();
    });
    return res;
  }


  value operator()(const ast::sel& self, const ref<env>& e) const {
    const symbol name = self.name;
    return builtin(1, [name](const value* args) -> value {
      return args[0].cast<record>().attrs.at(name);      
      // return args[0].cast<record>().attrs.find(name)->second;
    });
  }
  
  
  template<class T>
  value operator()(const T& self, const ref<env>& e) const {
	throw std::logic_error("eval unimplemented: " + tool::type_name(typeid(T)));
  }
  
};


static value eval(const ref<env>& e, const ast::io& self) {
  return self.visit<value>(eval_visitor(), e);
}

value eval(const ref<env>& e, const ast::expr& self) {
  return self.visit<value>(eval_visitor(), e);
}

  
value eval(const ref<env>& e, const ast::toplevel& self) {
  return self.visit<value>(eval_visitor(), e);
}


namespace {
struct ostream {
  template<class T>
  void operator()(const T& self, std::ostream& out) const {
	out << self;
  }

  void operator()(const lambda& self, std::ostream& out) const {
	out << "#<lambda>";
  }

  void operator()(const builtin& self, std::ostream& out) const {
	out << "#<builtin>";
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
