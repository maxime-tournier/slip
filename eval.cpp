#include "eval.hpp"

static value eval(const ref<env>& e, const ast::io& self);


value apply(const value& func, const value::list& args) {
  return func.match<value>
	( [&](const lambda& self) {
		return eval(augment(self.scope, self.args, args), *self.body);
	  },
	  [&](const builtin& self) {
		return self(args);
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
    const value::list args = map(self.args,
								 [&](const ast::expr& self) {
								   return eval(e, self);
								 });
	return apply(func, args);
  }
  
  value operator()(const ast::abs& self, const ref<env>& e) const {
	return lambda{self.args, self.body, e};
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

				   
  value operator()(const ast::io& self, const ref<env>& e) const {
	return self.visit<value>(eval_visitor(), e);
  }

  value operator()(const ast::expr& self, const ref<env>& e) const {
	return self.visit<value>(eval_visitor(), e);
  }


  
  template<class T>
  value operator()(const T& self, const ref<env>& e) const {
	throw std::logic_error("unimplemented: " + std::string(typeid(T).name()));
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
};
}

std::ostream& operator<<(std::ostream& out, const value& self) {
  self.visit(ostream(), out);
  return out;
}
