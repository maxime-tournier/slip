#include "unify.hpp"
#include "maybe.hpp"

#include "substitution.hpp"

#include <sstream>

namespace type {


  static maybe<extension> rewrite(state* s, symbol attr, mono row);
  static void occurs_check(state* self, var v, mono t);
  static void link(substitution* self, const var& from, const mono& to);
  static maybe<extension> rewrite(state* s, symbol attr, mono row);
  static void upgrade(state* self, mono t, std::size_t level, logger* log);
  
  // logging
  static std::size_t indent = 0;
  struct lock {
    lock() { ++indent; }
    ~lock() { --indent; }
  };


  static std::string prefix() {
    std::stringstream ss;
    for(std::size_t i = 0 ; i < indent; ++i) {
      ss << "  |";
    }

    ss << "--";
    
    return ss.str();
  };
  
  // link types in substitution
  static void link(substitution* sub, const var& from, const mono& to) {
    assert(from->kind == to.kind());
    if(mono(from) == to) return;

    const lock instance;
    sub->link(from, to);
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
      std::stringstream ss;
      
      logger(ss) << "type variable " << self->generalize(v)
                 << " occurs in type " << self->generalize(t);
      
      throw error(ss.str());
    }
  }

  

  
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

  // rewrite row to start with attr
  static maybe<extension> rewrite(state* s, symbol attr, mono row) {
    return row.visit(rewrite_visitor(), attr, s);
  };


  struct upgrade_visitor {
    using type = void;
    
    void operator()(const cst& self, std::size_t level, state* s, logger* log) const { }
    
    void operator()(const var& self, std::size_t level, state* s, logger* log) const {
      const auto sub = s->sub->substitute(self);

      if(sub != self) {
        sub.visit(upgrade_visitor(), level, s, log);
        return;
      }

      if(self->level > level) {
        const mono fresh = make_ref<variable>(level, self->kind);
        if(log) {
          *log << prefix() << "upgrading: " << s->generalize(self)  
               << " to level: " << level
               << " with: " << s->generalize(fresh)
               << std::endl;
        }
        const lock instance;
        s->unify(self, fresh, log);
        return;
      }
    }

    void operator()(const app& self, std::size_t level, state* s, logger* log) const {
      self->ctor.visit(upgrade_visitor(), level, s, log);
      self->arg.visit(upgrade_visitor(), level, s, log);      
    }
    
  };
  
  // make sure all substituted variables in a type have at most given level
  static void upgrade(state* self, mono t, std::size_t level, logger* log) {
    t.visit(upgrade_visitor(), level, self, log);
  }

  
  
  void unify_rows(state* self, substitution* sub,
                  const app& from, const app& to, logger* log) {
    
    const extension e = extension::unpack(from);

    // try rewriting 'to' like 'from'
    if(auto rw = rewrite(self, e.attr, to)) {

      if(log) {
        *log << prefix()
             << "rewrote: " << self->generalize(to)
             << " as: " << self->generalize(rw.get().head) << "; "
             << self->generalize(rw.get().tail)
             << std::endl;
      }
    
      // rewriting succeeded, unify rewritten terms
      unify_terms(self, sub, e.head, rw.get().head, log);

      // TODO switch tail order so that we don't always rewrite the same side
      unify_terms(self, sub, e.tail, rw.get().tail, log);
      return;
    }

    // rewriting failed: attribute error
    {
      std::stringstream ss;
      ss << "no attribute " << tool::quote(e.attr.get())
         << " in row type: " << tool::show(self->generalize(to));
      const unification_error e(ss.str());
      if(log) *log << prefix() << "error: " << e.what() << std::endl;
      throw e;
    }
  }


  void unify_terms(state* self, substitution* sub, mono from, mono to, logger* log) {
      
    using var = ref<variable>;
    using app = ref<application>;

    if(log) {
      *log << prefix()
           << "unifying: " << self->generalize(from)
           << " with: " << self->generalize(to)
           << std::endl;
    }

    const lock instance;
    
    // resolve
    from = sub->substitute(from);
    to = sub->substitute(to);

    if(from.kind() != to.kind()) {
      std::stringstream ss;
      ss << "cannot unify type: " << tool::show(self->generalize(from)) << std::endl
         << "...with type:\t " << tool::show(self->generalize(to));
        
      const unification_error e(ss.str());
      
      if(log) *log << prefix() << "kind error" << std::endl;
      throw e;
    }

    const kind::any k = from.kind();

    
    // var -> mono
    if(auto v = from.get<var>()) {
      occurs_check(self, *v, to);
      link(sub, *v, to);
      upgrade(self, to, (*v)->level, log);
      return;
    }

    // mono <- var
    if(auto v = to.get<var>()) {
      occurs_check(self, *v, from);
      link(sub, *v, from);
      upgrade(self, from, (*v)->level, log);
      return;
    }

    // app <-> app
    if(from.get<app>() && to.get<app>()) {

      // row polymorphism
      if(k == kind::row()) {
        unify_rows(self, sub, from.cast<app>(), to.cast<app>(), log);
        return;
      }

      unify_terms(self, sub, from.cast<app>()->arg, to.cast<app>()->arg, log);
      unify_terms(self, sub, from.cast<app>()->ctor, to.cast<app>()->ctor, log);
      return;
    }
  
    if(from != to) {
      std::stringstream ss;
      logger(ss) << "cannot unify types " << tool::show(self->generalize(from))
                 << " and " << tool::show(self->generalize(to));
      
      const unification_error e(ss.str());
      if(log) *log << prefix() << "error: " << e.what() << std::endl;
      throw e;
    }
  }
  

  
}
