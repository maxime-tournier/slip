#include "type.hpp"

#include <vector>
#include <sstream>

#include "ast.hpp"
#include "tool.hpp"



namespace type {

state& state::operator()(std::string s, mono t) {
  locals.emplace(s, generalize(t));
  return *this;
}
  

static mono instantiate(poly self, std::size_t depth);


static const auto make_constant = [](const char* name, kind::any k=kind::term) {
  return make_ref<constant>(constant{symbol(name), k});
};

static const auto make_variable = [](std::size_t level, kind::any k=kind::term) {
  return make_ref<variable>(variable{level, k});
};


ref<application> apply(mono ctor, mono arg) {
  return make_ref<application>(ctor, arg);
}

mono operator>>=(mono from, mono to) {
  return apply(apply(func, from), to);
}



application::application(mono ctor, mono arg)
  : ctor(ctor),
    arg(arg) {
  const kind::any k = ctor.kind();
  if(auto c = k.get<ref<kind::constructor>>()) {
    if((*c)->from != arg.kind()) {
      throw kind::error("argument has not the expected kind");
    }
  } else {
    throw kind::error("type constructor must have constructor kind");
  }
}

// constants
const ref<constant> unit = make_constant("unit");
const ref<constant> boolean = make_constant("boolean");
const ref<constant> integer = make_constant("integer");
const ref<constant> real = make_constant("real");

const ref<constant> func =
  make_constant("->", kind::term >>= kind::term >>= kind::term);

const ref<constant> io =
  make_constant("io", kind::term >>= kind::term);




struct kind_visitor {

  kind::any operator()(const ref<constant>& self) const {
    return self->kind;
  }

  kind::any operator()(const ref<variable>& self) const {
    return self->kind;
  }

  kind::any operator()(const ref<application>& self) const {
    const kind::any k = self->ctor.kind();
    if(auto c = k.get<ref<kind::constructor>>()) {
      return (*c)->to;
    } else {
      throw std::logic_error("derp");
    }
  }
      
};

kind::any mono::kind() const {
  return visit<kind::any>(kind_visitor());
}



struct infer_visitor {
  template<class T>
  mono operator()(const T& self, const ref<state>&) const {
    throw std::logic_error("infer unimplemented: " + tool::type_name(typeid(T)));
  }


  // var
  mono operator()(const ast::var& self, const ref<state>& s) const {
    try {
      return instantiate(s->find(self.name), s->level);
    } catch(std::out_of_range& e) {
      throw error("unbound variable " + tool::quote(self.name.get()));
    }
  }


  // abs
  mono operator()(const ast::abs& self, const ref<state>& s) const {
    std::vector<poly> vars;
    
    // construct function type
    const mono result = s->fresh();
    const mono sig = foldr(result, self.args, [&](symbol name, mono t) {
      const mono alpha = s->fresh();
      // note: alpha is monomorphic in sigma
      const poly sigma = {{}, alpha}; 
      vars.emplace_back(sigma);
      return alpha >>= t;
    });
    
    // infer lambda body with augmented environment
    auto scope = augment(s, self.args, vars.rbegin(), vars.rend());
    s->unify(result, mono::infer(scope, *self.body));
    
    return sig;
  }


  // app
  mono operator()(const ast::app& self, const ref<state>& s) const {
    const mono func = mono::infer(s, *self.func);
    const mono result = s->fresh();

    const mono sig = foldr(result, self.args, [&](ast::expr e, mono t) {
        return mono::infer(s, e) >>= t;
      });

    s->unify(sig, func);
    return result;
  }


  // let
  mono operator()(const ast::let& self, const ref<state>& s) const {
    auto sub = scope(s);
    for(ast::def def : self.defs) {
      sub->locals.emplace(def.name, s->generalize(mono::infer(s, def.value)));
    }

    return mono::infer(sub, *self.body);
  }

  
  // lit
  mono operator()(const ast::lit<::unit>& self, const ref<state>&) const {
    return unit;
  }

  mono operator()(const ast::lit<::boolean>& self, const ref<state>&) const {
    return boolean;
  }

  mono operator()(const ast::lit<::integer>& self, const ref<state>&) const {
    return integer;
  }

  mono operator()(const ast::lit<::real>& self, const ref<state>&) const {
    return real;
  }
  
};


// polytype instantiation
struct instantiate_visitor {
  const poly::forall_type& forall;
  const std::size_t level;
  
  mono operator()(const ref<constant>& self) const {
    return self;
  }

  mono operator()(const ref<variable>& self) const {
    auto it = forall.find(self);
    if(it != forall.end()) {
      return make_variable(level, self->kind);
    } else {
      return self;
    }
  }

  mono operator()(const ref<application>& self) const {
    return apply(self->ctor.visit<mono>(*this),
                 self->arg.visit<mono>(*this));
  }
  
};


static mono instantiate(poly self, std::size_t level) {
  return self.type.visit<mono>(instantiate_visitor{self.forall, level});
}


mono mono::infer(const ref<state>& s, const ast::expr& self) {
  return self.visit<mono>(infer_visitor(), s);
}


// type state
state::state()
  : level(0),
    sub(make_ref<substitution>()) {

}


state::state(const ref<state>& parent)
  : environment<poly>(parent),
    level(parent->level + 1),
    sub(parent->sub) {

}


ref<variable> state::fresh(kind::any k) const {
  return make_variable(level, k);
}





// generalize
struct generalize_visitor {
  const std::size_t level;
  
  template<class OutputIterator>
  void operator()(const ref<constant>&, OutputIterator) const { }

  template<class OutputIterator>
  void operator()(const ref<variable>& self, OutputIterator out) const {
    // generalize if variable is deeper than current level
    if(self->level >= level) {
      *out++ = self;
    }
  }

  template<class OutputIterator>
  void operator()(const ref<application>& self, OutputIterator out) const {
    self->ctor.visit(*this, out);
    self->arg.visit(*this, out);
  }
  
};


poly state::generalize(const mono& t) const {
  poly::forall_type forall;
  const mono s = substitute(t);
  s.visit(generalize_visitor{level}, std::inserter(forall, forall.begin()));
  return poly{forall, s};
}


struct stream_visitor {
  using map_type = std::map<ref<variable>, std::size_t>;
  map_type& map;

  void operator()(const ref<constant>& self, std::ostream& out,
                  const poly::forall_type& forall) const {
    out << self->name;
  }

  void operator()(const ref<variable>& self, std::ostream& out,
                  const poly::forall_type& forall) const {
    auto it = map.emplace(self, map.size()).first;
    if(forall.find(self) != forall.end()) {
      out << "'";
    } else {
      out << "!";
    }
    
    out << char('a' + it->second);
    // out << "(" << std::hex << (long(self.get()) & 0xffff) << ")";
  }

  void operator()(const ref<application>& self, std::ostream& out,
                  const poly::forall_type& forall) const {
    if(self->ctor == func) {
      self->arg.visit(*this, out, forall);
      out << " ";
      self->ctor.visit(*this, out, forall);
    } else { 
      // TODO parens
      self->ctor.visit(*this, out, forall);
      out << " ";
      self->arg.visit(*this, out, forall);
    }
  }
  
};


std::ostream& operator<<(std::ostream& out, const poly& self) {
  stream_visitor::map_type map;
  self.type.visit(stream_visitor{map}, out, self.forall);
  return out;
}


struct substitute_visitor {

  mono operator()(const ref<variable>& self, const state& s) const {
    auto it = s.sub->find(self);
    if(it == s.sub->end()) return self;
    assert(it->second != self);
    return s.substitute(it->second);
  }

  mono operator()(const ref<application>& self, const state& s) const {
    
    return apply(s.substitute(self->ctor), s.substitute(self->arg));
  }

  mono operator()(const ref<constant>& self, const state&) const {
    return self;
  }
  
};

mono state::substitute(const mono& t) const {
  return t.visit<mono>(substitute_visitor(), *this);
}

static std::size_t indent = 0;
struct lock {
  lock() { ++indent; }
  ~lock() { --indent; }
};

void state::unify(mono from, mono to) {
  const lock instance;

  using var = ref<variable>;
  using app = ref<application>;

  std::clog << std::string(2 * indent, '.') << "unifying: " << generalize(from)
            << " with: " << generalize(to) << std::endl;

  
  // resolve
  from = substitute(from);
  to = substitute(to);

  // var -> var
  if(from.get<var>() && to.get<var>()) {
    // var <- var
    if(from.cast<var>()->level < to.cast<var>()->level) {
      sub->emplace(from.cast<var>(), to);
      return;
    }
  }
  
  // var -> mono
  if(auto v = from.get<var>()) {
    sub->emplace(*v, to);
    return;
  }

  // mono <- var
  if(auto v = to.get<var>()) {
    sub->emplace(*v, from);
    return;
  }

  // app <-> app
  if(from.get<app>() && to.get<app>()) {
    unify(from.cast<app>()->ctor, to.cast<app>()->ctor);
    unify(from.cast<app>()->arg, to.cast<app>()->arg);
    return;
  }
  
  if(from != to) {
    std::stringstream ss;
    ss << "cannot unify: " << generalize(from) << " with: " << generalize(to);
    throw error(ss.str());
  }

}




};
