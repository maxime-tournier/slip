#include "type.hpp"

#include <vector>
#include <sstream>

#include "ast.hpp"
#include "tool.hpp"

#include "maybe.hpp"
#include "package.hpp"

namespace type {

static const bool debug = false;
  
static const auto make_constant = [](const char* name, kind::any k=kind::term()) {
  return make_ref<constant>(constant{name, k});
};

  
static const auto make_variable = [](std::size_t level, kind::any k=kind::term()) {
  return make_ref<variable>(variable{level, k});
};

template<class Func>
static void iter_rows(mono self, Func func);
  

// constants
const mono unit = make_constant("unit");
const mono boolean = make_constant("boolean");
const mono integer = make_constant("integer");
const mono real = make_constant("real");


  const mono func =
    make_constant("->", kind::term() >>= kind::term() >>= kind::term());

  const mono list =
    make_constant("list", kind::term() >>= kind::term());
  
  const mono io =
    make_constant("io", kind::term() >>= kind::term());
  
  // records
  const mono record =
    make_constant("record", kind::row() >>= kind::term());

  const mono empty = make_constant("{}", kind::row());
  
  mono ext(symbol attr) {
    static const kind::any k = kind::term() >>= kind::row() >>= kind::row();  
    static std::map<symbol, ref<constant>> table;
    const std::string name = attr.get() + std::string(":");
  
    auto it = table.emplace(attr, make_constant(name.c_str(), k));
    return it.first->second;
  }

  const mono ty = make_constant("type", kind::term());

state& state::def(symbol name, mono t) {
  if(!vars->locals.emplace(name, generalize(t)).second) {
    throw error("redefined variable " + tool::quote(name.get()));
  }
  
  return *this;
}
  


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
  if(auto c = k.get<kind::constructor>()) {
    if(*c->from != arg.kind()) {
      std::clog << poly{{}, ctor} << " :: " << ctor.kind()
                << " " << poly{{}, arg} << " :: " << arg.kind()
                << std::endl;
      throw kind::error("argument does not have the expected kind");
    }
  } else {
    throw kind::error("type constructor must have constructor kind");
  }
}


struct ostream_visitor {
  using type = void;
  
  using map_type = std::map<ref<variable>, std::size_t>;
  map_type& map;

  void operator()(const ref<constant>& self, std::ostream& out,
                  const poly::forall_type& forall, bool parens) const {
    out << self->name;
  }

  void operator()(const ref<variable>& self, std::ostream& out,
                  const poly::forall_type& forall, bool parens) const {
    auto it = map.emplace(self, map.size()).first;
    if(forall.find(self) != forall.end()) {
      out << "'";
    } else {
      out << "!";
    }
    
    out << char('a' + it->second);

    static const bool extra = false;
    if(debug && extra) {
      out << "("
          << std::hex << (long(self.get()) & 0xffff)
          << "::" << self->kind
          << ")";
    }
  }

  void operator()(const ref<application>& self, std::ostream& out,
                  const poly::forall_type& forall, bool parens) const {
    const kind::any k = mono(self).kind();
    
    if(self->ctor == record) {
      // skip record constructor altogether since row types have {} around
      self->arg.visit(*this, out, forall, true);
      return;
    }

    if(parens) {
      if(k == kind::row()) out << "{";
      else out << "(";
    }
    
    if(self->ctor == func) {
      // print function types in infix style
      self->arg.visit(*this, out, forall, true);
      out << " ";
      self->ctor.visit(*this, out, forall, false);
    } else if(mono(self).kind() == kind::row()) {
      // print row types: ctor is (attr: type) so we want parens
      self->ctor.visit(*this, out, forall, true);

      // skip trailing empty record type
      if(self->arg != empty) {
        out << " ";
        self->arg.visit(*this, out, forall, false);
      }
    } else {
      // regular types
      self->ctor.visit(*this, out, forall, false);
      out << " ";

      const auto func_rhs = [&] {
        if(auto a = self->ctor.get<app>()) {
          if((*a)->ctor == func) return true;
        }
        return false;
      };

      // don't put parens on function rhs
      self->arg.visit(*this, out, forall, !func_rhs());
    }

    if(parens) {
      if(k == kind::row()) out << "}";
      else out << ")";
    }
  }
  
};


struct show {
  ostream_visitor::map_type& map;
  const poly& p;

  show(ostream_visitor::map_type& map, const poly& p)
    : map(map), p(p) { }
  
  friend std::ostream& operator<<(std::ostream& out, const show& self) {
    self.p.type.visit(ostream_visitor{self.map}, out, self.p.forall, false);
    return out;
  }
};


  

std::ostream& operator<<(std::ostream& out, const poly& self) {
  ostream_visitor::map_type map;
  self.type.visit(ostream_visitor{map}, out, self.forall, false);
  return out;
}


  
struct kind_visitor {
  using type = kind::any;
  
  kind::any operator()(const ref<constant>& self) const {
    return self->kind;
  }

  kind::any operator()(const ref<variable>& self) const {
    return self->kind;
  }

  kind::any operator()(const ref<application>& self) const {
    const kind::any k = self->ctor.kind();
    if(auto c = k.get<kind::constructor>()) {
      return *c->to;
    } else {
      throw std::logic_error("derp");
    }
  }
      
};

kind::any mono::kind() const {
  return visit(kind_visitor());
}



  
  struct let {
    const ::list<ast::bind> defs;
    const ast::expr body;

    static symbol fix() { return "__fix__"; }
    
    // rewrite let as non-recursive let + fix **when defining functions**
    static let rewrite(const ast::let& self) {
      using ::list;
      const list<ast::bind> defs = map(self.defs, [](ast::bind self) {
          return self.value.match<ast::bind>
          ([&](const ast::abs& abs) { 
            return ast::bind{self.name,
                             ast::app{ast::var{fix()},
                                      ast::abs{self.name >>= list<ast::abs::arg>(),
                                          self.value}
                                      >>= list<ast::expr>()}};
          }, 
            [&](const ast::expr& expr) { return self; });
        });

      return {defs, *self.body};
    }
  };

struct infer_visitor {
  using type = mono;
  
  template<class T>
  mono operator()(const T& self, const ref<state>&) const {
    throw std::logic_error("infer unimplemented: " + tool::type_name(typeid(T)));
  }


  // var
  mono operator()(const ast::var& self, const ref<state>& s) const {
    try {
      return s->instantiate(s->vars->find(self.name));
    } catch(std::out_of_range& e) {
      throw error("unbound variable " + tool::quote(self.name.get()));
    }
  }


  // abs
  mono operator()(const ast::abs& self, const ref<state>& s) const {

    // function scope
    const auto sub = scope(s);

    // construct function type
    const mono result = s->fresh();
    
    const mono res = foldr(result, self.args, [&](ast::abs::arg arg, mono tail) {
      const mono sig = arg.match<mono>
        ([&](symbol self) {
          // untyped arguments: trivial signature
          const mono a = sub->fresh();
          return a >>= a;
        },
          [&](ast::abs::typed self) {
            return s->instantiate(s->sigs->find(self.type));
          });
      
      const mono outer = s->fresh();
      const mono inner = sub->fresh();
      
      sub->unify(outer >>= inner, sig);
      std::clog << "arg: " << arg.name() << std::endl;
      
      sub->def(arg.name(), inner);
      
      return outer >>= tail;
    });

    
    
    // infer lambda body with augmented environment
    s->unify(result, infer(sub, *self.body));
    
    return res;
  }


  // app
  mono operator()(const ast::app& self, const ref<state>& s) const {
    const mono func = infer(s, *self.func);
    const mono result = s->fresh();

    const mono sig = foldr(result, self.args, [&](ast::expr e, mono t) {
        return infer(s, e) >>= t;
      });

    s->unify(sig, func);
    return result;
  }


  // non-recursive let
  mono operator()(const let& self, const ref<state>& s) const {
    auto sub = scope(s);

    for(ast::bind def : self.defs) {
      sub->vars->locals.emplace(def.name, s->generalize(infer(s, def.value)));
    }
    
    return infer(sub, self.body);
  }
  
  // recursive let
  mono operator()(const ast::let& self, const ref<state>& s) const {
    auto sub = scope(s);

    const mono a = sub->fresh();
    sub->def(let::fix(), (a >>= a) >>= a);
    
    return operator()(let::rewrite(self), sub);
  }


  // sel
  mono operator()(const ast::sel& self, const ref<state>& s) const {
    const mono tail = s->fresh(kind::row());
    const mono head = s->fresh(kind::term());
    
    const mono row = ext(self.name)(head)(tail);
    return record(row) >>= head;
  }

  // record
  mono operator()(const ast::record& self, const ref<state>& s) const {
    const mono init = empty;
    const mono row = foldr(init, self.attrs, [&](ast::record::attr attr, mono tail) {
        return ext(attr.name)(infer(s, attr.value))(tail);
      });

    return record(row);
  }

  


  // make
  mono operator()(const ast::make& self, const ref<state>& s) const {
    // get signature type
    const poly sig = s->vars->find(self.type);

    auto sub = scope(s);
    
    const mono outer = s->fresh();
    const mono inner = sub->fresh();

    // instantiate signature at sub level prevents generalization of
    // contravariant side (variables only appearing in the covariant side will
    // be generalized)
    s->unify(ty(outer) >>= ty(inner), sub->instantiate(sig));
    
    // vanilla covariant type
    const poly reference = sub->generalize(inner);
    
    // build provided type
    const mono init = empty;
    const mono provided =
      record(foldr(init, self.attrs,
                [&](const ast::record::attr attr, mono tail) {
                  return row(attr.name, infer(sub, attr.value)) |= tail;
                }));

    // now also unify inner with provided type
    s->unify(inner, provided);

    // now generalize the provided type
    const poly gen = sub->generalize(inner);

    // generalization check: quantified variables in reference/gen should
    // substitute to the same variables
    std::set<var> quantified;
    for(const var& v : gen.forall) {
      assert(sub->substitute(v) == v);
      quantified.insert(v);
    }

    // make sure all reference quantified references substitute to quantified
    // variables
    for(const var& v : reference.forall) {
      const mono vs = sub->substitute(v);
      if(auto u = vs.get<var>()) {
        auto it = quantified.find(*u);
        if(it != quantified.end()) {
          continue;
        }
      }

      ostream_visitor::map_type map;
      std::stringstream ss;
      ss << "failed to generalize "
         << show(map, gen)
         << " as " 
         << show(map, reference);
        throw error(ss.str());
    }

    return outer;
  }
  

  // cond
  mono operator()(const ast::cond& self, const ref<state>& s) const {
    const mono test = infer(s, *self.test);
    s->unify(test, boolean);

    const mono conseq = infer(s, *self.conseq);
    const mono alt = infer(s, *self.alt);    

    const mono result = s->fresh();
    s->unify(result, conseq);
    s->unify(result, alt);

    return result;    
  }

  
  // def
  mono operator()(const ast::def& self, const ref<state>& s) const {
    using ::list;
    const mono value =
      infer(s, ast::let(ast::bind{self.name, *self.value} >>= list<ast::bind>(),
                              ast::var{self.name}));
    try {
      s->def(self.name, value);
      return io(unit);
    } catch(std::runtime_error& e) {
      throw error(e.what());
    }
  }


  // use
  mono operator()(const ast::use& self, const ref<state>& s) const {
    // infer value type
    const mono value = infer(s, *self.env);

    // make sure value type is a record
    const mono row = s->fresh(kind::row());
    s->unify(value, record(row));

    auto sub = scope(s);
    
    // fill sub scope with record contents
    iter_rows(s->substitute(row), [&](symbol attr, mono t) {
        // TODO generalization issue?
        sub->def(attr, t);
      });

    return infer(sub, *self.body);
  }


  // import
  mono operator()(const ast::import& self, const ref<state>& s) const {
    auto it = s->vars->locals.find(self.package);
    if(it != s->vars->locals.end()) {
      throw error("variable " + tool::quote(self.package.get()) + " already defined");
    }
    
    const package pkg = package::import(self.package);
    
    s->vars->locals.emplace(self.package, pkg.sig());
    return io(unit);
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
  using type = mono;
  
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
    return self->ctor.visit(*this)(self->arg.visit(*this));
  }
  
};


mono state::instantiate(const poly& self) const {
  // associate quantified variables with fresh variables
  instantiate_visitor::map_type map;
  for(const ref<variable>& it : self.forall) {
    // assert(it->level <= level);
    map.emplace(it, make_ref<variable>( variable{level, it->kind} ));
  }

  // instantiate
  return self.type.visit(instantiate_visitor{map});
}


mono infer(const ref<state>& s, const ast::expr& self) {
  return self.visit(infer_visitor(), s);
}


// type state
state::state()
  : level(0),
    vars(make_ref<vars_type>()),
    sigs(make_ref<sigs_type>()),
    sub(make_ref<substitution>()) {

}


state::state(const ref<state>& parent)
  : level(parent->level + 1),
    vars(make_ref<vars_type>(parent->vars)),
    sigs(make_ref<sigs_type>(parent->sigs)),
    sub(parent->sub)
{

}


ref<variable> state::fresh(kind::any k) const {
  return make_variable(level, k);
}





// generalize
struct generalize_visitor {
  using type = void;
  
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




struct substitute_visitor {
  using type = mono;
  
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
  return t.visit(substitute_visitor(), *this);
}


static std::size_t indent = 0;
struct lock {
  lock() { ++indent; }
  ~lock() { --indent; }
};



static void link(state* self, const var& from, const mono& to) {
  assert(from->kind == to.kind());
  if(mono(from) == to) return;
  
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
    const std::string name = std::string(row->name.get());
    
    const symbol attr(name.substr(0, name.size() - 1));
    return {attr, head, tail};
  }
};

template<class Func>
static void iter_rows(mono self, Func func) {
  assert(self.kind() == kind::row());
  self.match
    ([&](const app& self) {
      const auto e = extension::unpack(self);
      func(e.attr, e.head);
      iter_rows(e.tail, func);
    }, [](const mono& ) { });
}

static maybe<extension> rewrite(state* s, symbol attr, mono row);

struct rewrite_visitor {
  using type = maybe<extension>;
  
  type operator()(const cst& self, symbol attr, state* s) const {
    assert(mono(self) == empty);
    return {};
  }

  type operator()(const var& self, symbol attr, state* s) const {
    // TODO unify self / ext(attr)(alpha)(rho) here?
    return extension{attr, s->fresh(kind::term()), s->fresh(kind::row())};
  }

  type operator()(const app& self, symbol attr, state* s) const {
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
  return row.visit(rewrite_visitor(), attr, s);
};

  static void unify(state* self, mono from, mono to, ostream_visitor::map_type& map);
  
  static void unify_rows(state* self, const app& from, const app& to,
                         ostream_visitor::map_type& map) {
  const extension e = extension::unpack(from);

  // try rewriting 'to' like 'from'
  if(auto rw = rewrite(self, e.attr, to)) {

    if(debug) {
      std::clog << std::string(2 * indent, '.')
                << "rewrote: " << show(map, self->generalize(to))
                << " as: " << show(map, self->generalize(rw.get().head)) << "; "
                << show(map, self->generalize(rw.get().tail))
                << std::endl;
    }
    
    // rewriting succeeded, unify rewritten terms
    unify(self, e.head, rw.get().head, map);

    // TODO switch tail order so that we don't always rewrite the same side
    unify(self, e.tail, rw.get().tail, map);
    return;
  }

  // rewriting failed: attribute error
  std::stringstream ss;
  ss << "expected attribute " << tool::quote(e.attr.get())
     << " in record type \"" << self->generalize(to) << "\"";
  throw error(ss.str());
}


  struct upgrade_visitor {
    using type = void;
    
    void operator()(const cst& self, std::size_t level, state* s) const { }
    void operator()(const var& self, std::size_t level, state* s) const {
      const auto sub = s->substitute(self);

      if(sub != self) {
        sub.visit(upgrade_visitor(), level, s);
        return;
      }

      if(self->level > level) {
        s->unify(self, make_variable(level, self->kind));
        return;
      }
    }

    void operator()(const app& self, std::size_t level, state* s) const {
      self->ctor.visit(upgrade_visitor(), level, s);
      self->arg.visit(upgrade_visitor(), level, s);      
    }
    
  };
  
  // make sure all substituted variables in a type have at most given level
  static void upgrade(state* self, mono t, std::size_t level) {
    t.visit(upgrade_visitor(), level, self);
  }


  struct occurs_visitor {
    using type = bool;

    type operator()(const cst& self, const var& v) const {
      return false;
    }
    
    type operator()(const var& self, const var& v) const {
      return self == v;
    }
    
    type operator()(const app& self, const var& v) const {
      return self->ctor.visit(occurs_visitor(), v) ||
        self->arg.visit(occurs_visitor(), v);
    }
  };
  
  static void occurs_check(state* self, var v, mono t) {
    if(mono(v) == t) return;
    
    if(t.visit(occurs_visitor(), v)) {
      ostream_visitor::map_type map;
      std::stringstream ss;
      ss << "type variable " << show(map, self->generalize(v))
         << " occurs in type " << show(map, self->generalize(t));
      
      throw error(ss.str());
    }
  }



  static void unify(state* self, mono from, mono to, ostream_visitor::map_type& map) {
    const lock instance;

    using var = ref<variable>;
    using app = ref<application>;

    if(debug) {
      std::clog << std::string(2 * indent, '.')
                << "unifying: " << show(map, self->generalize(from))
                << " with: " << show(map, self->generalize(to))
                << std::endl;
    }
  
    // resolve
    from = self->substitute(from);
    to = self->substitute(to);

    if(from.kind() != to.kind()) {
      throw kind::error("cannot unify types of different kinds");
    }

    const kind::any k = from.kind();
  
    // var -> mono
    if(auto v = from.get<var>()) {
      occurs_check(self, *v, to);
      link(self, *v, to);
      upgrade(self, to, (*v)->level);
      return;
    }

    // mono <- var
    if(auto v = to.get<var>()) {
      occurs_check(self, *v, from);
      link(self, *v, from);
      upgrade(self, from, (*v)->level);
      return;
    }

    // app <-> app
    if(from.get<app>() && to.get<app>()) {

      // row polymorphism
      if(k == kind::row()) {
        unify_rows(self, from.cast<app>(), to.cast<app>(), map);
        return;
      }

      unify(self, from.cast<app>()->arg, to.cast<app>()->arg, map);
      unify(self, from.cast<app>()->ctor, to.cast<app>()->ctor, map);
      return;
    }
  
    if(from != to) {
      ostream_visitor::map_type map;
      std::stringstream ss;
      ss << "cannot unify types \"" << show(map, self->generalize(from))
         << "\" and \"" << show(map, self->generalize(to)) << "\"";
      throw error(ss.str());
    }
  }
  
  void state::unify(mono from, mono to) {
    ostream_visitor::map_type map;
    type::unify(this, from, to, map);
  }


};
