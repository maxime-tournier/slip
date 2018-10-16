#include "infer.hpp"
#include "ast.hpp"
#include "package.hpp"

#include <sstream>

namespace type {


  // try to open type `self` from signatures in `s`
  static mono open(const ref<state>& s, const mono& self);

  
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


  // obtain constructor for a monotype
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

  
  // try to open type `self` from signatures in `s`
  static mono open(const ref<state>& s, const mono& self) {
    // obtain type constructor from argument type
    const cst c = constructor(s->substitute(self));
    
    try {
      auto sig = s->sigs->at(c);
        
      const mono inner = s->fresh();
      s->unify(self >>= inner, s->instantiate(sig));
      
      return inner;
    } catch(std::out_of_range&) {
      std::stringstream ss;
      ss << "constructor " << tool::quote(c->name.get()) << " has not associated signature";
      throw error(ss.str());
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  
  // error: unimplemented
  template<class T>
  static mono infer(const ref<state>& s, const T& self) {
    throw std::logic_error("infer unimplemented: " + tool::type_name(typeid(T)));
  }

  
  // var
  static mono infer(const ref<state>& s, const ast::var& self) {
    try {
      return s->instantiate(s->vars->find(self.name));
    } catch(std::out_of_range& e) {
      throw error("unbound variable " + tool::quote(self.name.get()));
    }
  }



  // initialize function scope with argument types
  static ref<state> function_scope(const ref<state>& s, const list<ast::abs::arg>& args) {
    auto sub = scope(s);
    
    for(auto arg: args) {
      const mono type = arg.match<mono>([&](symbol self) {
          return s->fresh();
        },
        [&](ast::abs::typed self) {
          // obtain reified type from annoatation
          const mono reified = infer(s, self.type);
          
          // TODO do we need gen/inst here?
          
          // extract concrete type from reified type
          const mono concrete = reconstruct(s, s->substitute(reified));
          
          return concrete;
        });
      
      sub->def(arg.name(), type);
    }
    
    return sub;
  }

  
  
  // abs
  static mono infer(const ref<state>& s, const ast::abs& self) {

    // function scope
    const auto sub = function_scope(s, self.args);

    const mono result = s->fresh();    
    const mono sig = foldr(result, self.args, [&](auto arg, mono rhs) {
        const mono type = s->instantiate(sub->vars->find(arg.name()));
        return type >>= rhs;
      });
    
    // infer lambda body with augmented environment
    s->unify(result, infer(sub, *self.body));
    
    return result;
  }
  
  


  // app
  static mono infer(const ref<state>& s, const ast::app& self) {
    // normalize application as unary
    const ast::app rw = rewrite(self);
    assert(size(rw.args) == 1);

    // infer func/arg types for application
    const auto with_inferred = [&](auto cont) {
      const mono func = infer(s, *rw.func);
      const mono arg = infer(s, rw.args->head);

      return cont(func, arg);
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
            return check(open(s, func), arg);
          });
      }
      catch(error& ) {  }

      try {
        // open arg type and retry        
        return with_inferred([&](mono func, mono arg) {
            return check(func, open(s, arg));
          });
      }
      catch(error& ) {  }

      try {
        // open both func and arg types and retry        
        return with_inferred([&](mono func, mono arg) {
            return check(open(s, func), open(s, arg));
          });
      }
      catch(error& ) {  }
      
      throw e;
    }
    
  }


  // non-recursive let
  static mono infer(const ref<state>& s, const let& self) {
    auto sub = scope(s);

    for(ast::bind def : self.defs) {
      sub->vars->locals.emplace(def.name, s->generalize(infer(s, def.value)));
    }
    
    return infer(sub, self.body);
  }

  
  // recursive let
  static mono infer(const ref<state>& s, const ast::let& self) {
    auto sub = scope(s);

    const mono a = sub->fresh();
    sub->def(let::fix(), (a >>= a) >>= a);
    
    return infer(sub, let::rewrite(self));
  }


  // sel
  static mono infer(const ref<state>& s, const ast::sel& self) {
    const mono tail = s->fresh(kind::row());
    const mono head = s->fresh(kind::term());
    
    const mono row = ext(self.name)(head)(tail);
    return record(row) >>= head;
  }

  // record attrs
  static mono infer(const ref<state>& s, const list<ast::record::attr>& attrs) {
    const mono init = empty;
    return foldr(init, attrs, [&](ast::record::attr attr, mono tail) {
        return ext(attr.name)(infer(s, attr.value))(tail);
      });
  }
  
  // record
  static mono infer(const ref<state>& s, const ast::record& self) {
    return record(infer(s, self.attrs));
  }

  


  // make
  static mono infer(const ref<state>& s, const ast::make& self) {
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
  static mono infer(const ref<state>& s, const ast::cond& self) {
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
  static mono infer(const ref<state>& s, const ast::def& self) {
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
  static mono infer(const ref<state>& s, const ast::use& self) {
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
  static mono infer(const ref<state>& s, const ast::import& self) {
    auto it = s->vars->locals.find(self.package);
    if(it != s->vars->locals.end()) {
      throw error("variable " + tool::quote(self.package.get()) + " already defined");
    }
    
    const package pkg = package::import(self.package);
    
    s->vars->locals.emplace(self.package, pkg.sig());
    return io(unit);
  }
  

  // module
  static mono infer(const ref<state>& s, const ast::module& self) {

    // module scope
    const auto sub = function_scope(s, self.args);

    const mono result = s->fresh();    
    const mono sig = foldr(result, self.args, [&](auto arg, mono rhs) {
        const mono type = s->instantiate(sub->vars->find(arg.name()));
        return type >>= rhs;
      });

    // infer type for module attributes in module scope
    const mono attrs = infer(sub, self.attrs);

    iter_rows(attrs, [&](symbol attr, mono reified) {
        // make sure each attribute has a reified type
        // const mono type = reconstruct(sub, reified);
      });

    // TODO compute kind from signature

    // TODO create a new type constructor with given name/computed kind

    // in the meantime, return signature
    s->unify(result, attrs);
    return sig;
  }


  
  // lit
  static mono infer(const ref<state>&, const ast::lit<::boolean>& self) {
    return boolean;
  }

  static mono infer(const ref<state>&, const ast::lit<::integer>& self) {
    return integer;
  }

  static mono infer(const ref<state>&, const ast::lit<::real>& self) {
    return real;
  }

  

  ////////////////////////////////////////////////////////////////////////////////
  
  struct infer_visitor {
    using type = mono;
  
    template<class T>
    mono operator()(const T& self, const ref<state>& s) const {
      return infer(s, self);
    }
    
  };
  

  mono infer(const ref<state>& s, const ast::expr& self) {
    return self.visit(infer_visitor(), s);
  }

  
}
