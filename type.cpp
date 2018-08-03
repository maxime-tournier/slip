#include "type.hpp"

#include "ast.hpp"
#include "tool.hpp"

#include <vector>

namespace kind {

// constants
const ref<constant> term = make_ref<constant>(constant{symbol{"*"}});
const ref<constant> row = make_ref<constant>(constant{symbol{"row"}});

any operator>>=(any from, any to) {
  return make_ref<constructor>(constructor{from, to});
}

}


namespace type {

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
    const auto result = s->fresh();
    
    std::vector<poly> vars;
    
    // construct function type
    const mono func = foldr(mono(result), self.args, [&](symbol name, mono t) {
      const mono alpha = s->fresh();
      vars.emplace_back(poly::generalize(alpha, s->level));
      return alpha >>= t;
    });
    
    // infer lambda body with augmented environment
    auto sub = augment(s, self.args, vars.rbegin(), vars.rend());
    s->subst->link(result, mono::infer(sub, *self.body));

    return func;
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

std::ostream& operator<<(std::ostream& out, const mono& self) {
  self.match([&](ref<constant> self) { out << self->name; },
             [&](ref<variable> self) { out << "#" << std::hex
                                           << (long(self.get()) & 0xffff); },
             [&](ref<application> self) {
               out << self->ctor << " " << self->arg;
             });
  return out;
}


// type state
state::state()
  : level(0),
    subst(make_ref<substitution>()) {

}

state::state(const ref<state>& parent)
  : environment<poly>(parent),
    level(parent->level + 1),
    subst(parent->subst) {

}

ref<variable> state::fresh(kind::any k) const {
  return make_variable(level, k);
}


// substitutions
void substitution::link(key_type from, mono to) {
  auto err = map.emplace(from, to);
  assert(err.second);
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


poly poly::generalize(const mono& self, std::size_t level) {
  forall_type forall;
  self.visit(generalize_visitor{level}, std::inserter(forall, forall.begin()));
  return poly{forall, self};
}




};
