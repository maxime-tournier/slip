#include "type.hpp"

#include <vector>
#include <sstream>

#include "ast.hpp"
#include "tool.hpp"

#include "maybe.hpp"

namespace type {

static const bool debug = false;

static const auto make_constant = [](const char* name, kind::any k=kind::term()) {
  return make_ref<constant>(constant{symbol(name), k});
};

static const auto make_variable = [](std::size_t level, kind::any k=kind::term()) {
  return make_ref<variable>(variable{level, k});
};

// constants
const mono unit = make_constant("unit");
const mono boolean = make_constant("boolean");
const mono integer = make_constant("integer");
const mono real = make_constant("real");


const mono func =
  make_constant("->", kind::term() >>= kind::term() >>= kind::term());


const mono io =
  make_constant("io", kind::term() >>= kind::term());


const mono rec =
  make_constant("record", kind::row() >>= kind::term());

const mono empty = make_constant("{}", kind::row());


mono ext(symbol attr) {
  static const kind::any k = kind::term() >>= kind::row() >>= kind::row();  
  static std::map<symbol, ref<constant>> table;
  const std::string name = std::string("@") + attr.get();
  
  auto it = table.emplace(attr, make_constant(name.c_str(), k));
  return it.first->second;
}


state& state::operator()(std::string s, mono t) {
  locals.emplace(s, generalize(t));
  return *this;
}
  

static mono instantiate(poly self, std::size_t depth);


mono mono::operator()(mono arg) const {
  return make_ref<application>(*this, arg);
}

mono operator>>=(mono from, mono to) {
  return mono(func)(from)(to);
}



application::application(mono ctor, mono arg)
  : ctor(ctor),
    arg(arg) {
  const kind::any k = ctor.kind();
  if(auto c = k.get<ref<kind::constructor>>()) {
    if((*c)->from != arg.kind()) {
      std::clog << poly{{}, ctor} << " :: " << ctor.kind()
                << " " << poly{{}, arg} << " :: " << arg.kind()
                << std::endl;
      throw kind::error("argument has not the expected kind");
    }
  } else {
    throw kind::error("type constructor must have constructor kind");
  }
}


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
      auto it = sub->locals.emplace(def.name,
                                    s->generalize(mono::infer(s, def.value))).first;
      (void) it;
      // std::clog << it->first << " : " << it->second << std::endl;
    }

    return mono::infer(sub, *self.body);
  }


  // sel
  mono operator()(const ast::sel& self, const ref<state>& s) const {
    const mono tail = s->fresh(kind::row());
    const mono head = s->fresh(kind::term());
    
    const mono row = ext(self.name)(head)(tail);
    return rec(row);
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
  using map_type = std::map<ref<variable>, ref<variable>>;
  const map_type& map;
  
  mono operator()(const ref<constant>& self) const {
    return self;
  }

  mono operator()(const ref<variable>& self) const {
    auto it = map.find(self);
    if(it != map.end()) {
      return it->second;
    } else {
      return self;
    }
  }

  mono operator()(const ref<application>& self) const {
    return self->ctor.visit<mono>(*this)(self->arg.visit<mono>(*this));
  }
  
};


static mono instantiate(poly self, std::size_t level) {
  // associate quantified variables with fresh variables
  instantiate_visitor::map_type map;
  for(const ref<variable>& it : self.forall) {
    assert(it->level <= level);
    map.emplace(it, make_ref<variable>( variable{level, it->kind} ));
  }

  // instantiate
  return self.type.visit<mono>(instantiate_visitor{map});
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


struct ostream_visitor {
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

    if(debug) {
      out << "(" << std::hex << (long(self.get()) & 0xffff) << ")";
    }
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
  ostream_visitor::map_type map;
  self.type.visit(ostream_visitor{map}, out, self.forall);
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
    return s.substitute(self->ctor)(s.substitute(self->arg));
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


struct show {
  ostream_visitor::map_type& map;
  const poly& p;
  
  friend std::ostream& operator<<(std::ostream& out, const show& self) {
    self.p.type.visit(ostream_visitor{self.map}, out, self.p.forall);
    return out;
  }
};



static void link(state* self, const ref<variable>& from, const mono& to) {
  assert(from->kind == to.kind());
  if(!self->sub->emplace(from, to).second) {
    assert(false);
  }
}

// helper structure for destructuring row extensions
struct extension {
  const symbol attr;
  const mono head;
  const mono tail;
  
  static extension unpack(const app& self) {
    // peel application of the form: ext(name)(head)(tail)
    const mono tail = self->arg;
    const app ctor = self->ctor.cast<app>();
    const mono head = ctor->arg;
    const cst row = ctor->ctor.cast<cst>();
    const symbol attr = row->name;
    return {attr, head, tail};
  }
};


static maybe<extension> rewrite(state* s, symbol attr, mono row);

struct rewrite_visitor {
  maybe<extension> operator()(const cst& self, symbol attr, state* s) const {
    assert(mono(self) == empty);
    return {};
  }

  maybe<extension> operator()(const var& self, symbol attr, state* s) const {
    assert(mono(self) == empty);
    // TODO unify self / ext(attr)(alpha)(rho) here?
    return extension{attr, s->fresh(kind::term()), s->fresh(kind::row())};
  }

  maybe<extension> operator()(const app& self, symbol attr, state* s) const {
    const extension e = extension::unpack(self);
    
    // attribute check
    if(e.attr == attr) return e;

    // try rewriting tail and proceed
    return rewrite(s, attr, e.tail) >> [&](extension sub) -> maybe<extension> {
      assert(sub.attr == attr);
      return extension{attr, sub.head, ext(e.attr)(e.head)(sub.tail)};
    };
  }
};


static maybe<extension> rewrite(state* s, symbol attr, mono row) {
  return row.visit< maybe<extension> >(rewrite_visitor(), attr, s);
};


void state::unify(mono from, mono to) {
  const lock instance;

  using var = ref<variable>;
  using app = ref<application>;

  if( debug ) {
    ostream_visitor::map_type map;
    std::clog << std::string(2 * indent, '.')
              << "unifying: " << show{map, generalize(from)}
    << " with: " << show{map, generalize(to)}
    << std::endl;
  }
  
  // resolve
  from = substitute(from);
  to = substitute(to);

  // var -> var
  if(from.get<var>() && to.get<var>()) {
    // var <- var
    if(from.cast<var>()->level < to.cast<var>()->level) {
      link(this, from.cast<var>(), to);
      return;
    }
  }
  
  // var -> mono
  if(auto v = from.get<var>()) {
    link(this, *v, to);
    return;
  }

  // mono <- var
  if(auto v = to.get<var>()) {
    link(this, *v, from);
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
    ss << "cannot unify types \"" << generalize(from) << "\" and \"" << generalize(to) << "\"";
    throw error(ss.str());
  }

}




};
