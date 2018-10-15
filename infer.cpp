#include "infer.hpp"
#include "ast.hpp"
#include "package.hpp"

#include <sstream>

namespace type {

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


  // rewrite app as nested unary applications
  static ast::app rewrite(const ast::app& self) {
    const ast::expr& init = *self.func;
    return foldl(init, self.args, [](ast::expr func, ast::expr arg) {
        return ast::app(func, arg >>= list<ast::expr>());
      }).cast<ast::app>();
  }


  
  // reconstruct actual type from reified type: ... -> (type 'a) yields 'a
  // note: t must be substituted
  static mono reconstruct(ref<state> s, mono t) {
    // std::clog << "reconstructing: " << s->generalize(t) << std::endl;
    
    if(auto self = t.get<app>()) {
      // std::clog << "ctor: " << s->generalize((*self)->ctor) << std::endl;
      
      if((*self)->ctor == ty) {
        return (*self)->arg;
      }
    }
    
    auto from = s->fresh();
    auto to = s->fresh();
    
    s->unify(from >>= to, t);
    return reconstruct(s, s->substitute(to));
  }
  
  static cst constructor(mono t) {
    if(auto self = t.get<app>()) {
      return constructor((*self)->ctor);
    }

    if(auto self = t.get<cst>()) {
      return *self;
    }

    // TODO restrictive?
    throw error("constructor must be a constant");
  }

  
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
            return a;
          },
            [&](ast::abs::typed self) {
              // obtain reified type from annoatation
              const mono t = infer(s, self.type);

              // extract actual type from reified
              const mono r = reconstruct(s, s->substitute(t));

              // obtain type constructor from type
              const cst c = constructor(r);

              try {
                // fetch associated signature, if any
                const auto sig = s->sigs->at(c);

                // instantiate signature
                return sub->instantiate(sig);
              
              } catch(std::out_of_range&) {
                throw error("unknown signature " + tool::quote(c->name.get()));
              }
            });
      
          const mono outer = s->fresh();
          const mono inner = sub->fresh();
      
          sub->unify(outer >>= inner, sig);
          sub->def(arg.name(), outer);

          // std::clog << "inner: " << sub->vars->find(arg.name()) << std::endl;
      
          return outer >>= tail;
        });
      
    
      // infer lambda body with augmented environment
      s->unify(result, infer(sub, *self.body));
    
      return res;
    }


    // app
    mono operator()(const ast::app& self, const ref<state>& s) const {
      // normalize application as unary
      const ast::app rw = rewrite(self);
      assert(size(rw.args) == 1);

      // infer func/arg types for application
      const auto with_inferred = [&](auto cont) {
        const mono func = infer(s, *rw.func);
        const mono arg = infer(s, rw.args->head);

        return cont(func, arg);
      };

      // obtain inner type from a type with associated signature
      const auto inner_type = [&](mono t) {
        // obtain type constructor from argument type
        const cst c = constructor(s->substitute(t));

        try {
          auto sig = s->sigs->at(c);
        
          const mono inner = s->fresh();
          s->unify(t >>= inner, s->instantiate(sig));
        
          return inner;
        } catch(std::out_of_range&) {
          throw error("unknown signature");
        }
      };

      // check if application works with given func/arg type
      const auto check = [&](mono func, mono arg) {
        const mono ret = s->fresh();
        s->unify(func , arg >>= ret);
        return ret;
      };

      // TODO find a less stupid way of trying all cases?
      try {
        // normal case
        return with_inferred([&](mono func, mono arg) {
            return check(func, arg);
          });
      
      } catch(error& e) {

        try {
          // open func type and retry
          return with_inferred([&](mono func, mono arg) {
              const mono inner_func = inner_type(func);
              return check(inner_func, arg);
            });
        }
        catch(error& ) {  }

        try {
          // open arg type and retry        
          return with_inferred([&](mono func, mono arg) {
              const mono inner_arg = inner_type(arg);
              return check(func, inner_arg);
            });
        }
        catch(error& ) {  }

        try {
          // open both func and arg types and retry        
          return with_inferred([&](mono func, mono arg) {
              const mono inner_func = inner_type(func);          
              const mono inner_arg = inner_type(arg);
              return check(inner_func, inner_arg);
            });
        }
        catch(error& ) {  }
      
        throw e;
      }

    
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

        std::stringstream ss;
        logger(ss) << "failed to generalize " << gen 
                   << " as " << reference;
        
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
  

  mono infer(const ref<state>& s, const ast::expr& self) {
    return self.visit(infer_visitor(), s);
  }

  
}
