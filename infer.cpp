#include "infer.hpp"
#include "ast.hpp"
#include "package.hpp"

#include "substitution.hpp"

#include <sstream>

namespace type {


  // try to open type `self` from signatures in `s`
  static mono open(const ref<state>& s, const mono& self);


  // foldr over row types
  template<class Init, class Func>
  static Init foldr_rows(const Init& init, mono self, const Func& func) {
    assert(self.kind() == kind::row());
    return self.match([&](const app& self) {
      const auto e = extension::unpack(self);
      return func(e.attr, e.head, foldr_rows(init, e.tail, func));
    }, [&](const mono& ) { return init; });
  }


  // foldr over function argument types. func is **not** called on result
  // type. note: you need to substitute in calls to func
  template<class Init, class Func>
  static Init foldr_args(const ref<state>& s,
                         const Init& init, mono self, const Func& func) {
    assert(self.kind() == kind::term());
    return s->sub->substitute(self).match([&](const app& self) {
      const mono from = s->fresh();
      const mono to = s->fresh();

      try {
        s->unify(from >>= to, self);
      } catch(error& e) {
        // TODO don't use exceptions for control flow ffs
        return init;
      }
      
      return func(from, foldr_args(s, init, to, func));
    }, [&](const mono& self) {
      return init;
    } );
  }
  
  
  template<class Init, class Func>
  static Init foldl_kind(const Init& init, const kind::any& self, const Func& func) {
    return self.match([&](const kind::constructor& self) {
        return foldl_kind(func(init, *self.from), *self.to, func);
      }, [&](const kind::constant& ) { return init; });
  }

  
  struct let {
    const ::list<ast::bind> defs;
    const ast::expr body;

    static symbol fix() { return "__fix__"; }
    
    // rewrite let as non-recursive let + fix **when defining functions**
    static let rewrite(const ast::let& self) {
      using ::list;
      const list<ast::bind> defs = map(self.defs, [](ast::bind self) {
          return self.value.match([&](const ast::abs& abs) { 
              return ast::bind{self.id,
                               ast::app{ast::var{fix()},
                                        ast::abs{self.id >>= list<ast::abs::arg>(),
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

    // turn nullary applications into unary applications
    ast::expr::list args = self.args;
    if(!args) args = ast::lit<::unit>{} >>= args;
                
    return foldl(init, args, [](ast::expr func, ast::expr arg) {
        return ast::app(func, arg >>= list<ast::expr>());
      }).cast<ast::app>();
  }


  
  // reconstruct actual type from reified type: ... -> (type 'a) yields 'a
  static mono reconstruct(ref<state> s, mono self) {
    self = s->sub->substitute(self);
    auto a = s->fresh();

    try {
      s->unify(ty(a), self);
      return a;
      // TODO don't use exceptions for control flow lol
    } catch(error& e) {
      auto from = s->fresh();
      auto to = s->fresh();
      
      s->unify(from >>= to, self);
      return reconstruct(s, to);
    }
  }


  // obtain constructor for a monotype
  static cst constructor(const ref<state>& s, mono t) {
    t = s->sub->substitute(t);
    
    if(auto self = t.get<app>()) {
      return constructor(s, (*self)->ctor);
    }

    if(auto self = t.get<cst>()) {
      return *self;
    }

    // TODO restrictive?
    throw error("constructor must be a constant");
  }


  // get signature associated with constructor `self`
  static poly signature(const ref<state>& s, const cst& self) {
    // find associated signature
    try {
      const poly sig = s->sigs->at(self);
      return sig;
    } catch(std::out_of_range&) {
      throw error("constructor " + tool::quote(self->name.get())
                  + " has no associated signature");
    }

  }
  
  
  // try to open type `self` from signatures in `s`
  static mono open(const ref<state>& s, const mono& self) {
    // obtain type constructor from argument type
    const cst ctor = constructor(s, self);

    // get signature
    const poly sig = signature(s, ctor);
    
    const mono inner = s->fresh();
    s->unify(self >>= inner, s->instantiate(sig));
    
    return inner;
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
  static ref<state> function_scope(const ref<state>& s,
                                   const list<ast::abs::arg>& args) {
    auto sub = scope(s);
    
    for(auto arg: args) {
      const mono type = arg.match([&](ast::var self) {
          return s->fresh();
        },
        [&](ast::abs::typed self) {
          // obtain reified type from annoatation
          const mono reified = infer(s, self.type);
          
          // TODO do we need gen/inst here?
          
          // extract concrete type from reified type
          const mono concrete = reconstruct(s, reified);
          
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


    // TODO not quite sure about this
    
    return sig;
  }
  
  


  // app
  static mono infer(const ref<state>& s, const ast::app& self) {
    // normalize application as unary
    const ast::app rw = rewrite(self);
    assert(size(rw.args) == 1);

    const mono func = infer(s, *rw.func);
    const mono arg = infer(s, rw.args->head);
    const mono ret = s->fresh();

    // TODO less stupid
    try {
      std::clog << "regular" << std::endl;
      s->unify(func, arg >>= ret);
      return ret;
    } catch(error& e) { 

      try {
        std::clog << "opening func" << std::endl;
        s->unify(open(s, func), arg >>= ret);
        return ret;
      } catch(error& ) { }

      try {
        std::clog << "opening arg" << std::endl;
        s->unify(func, open(s, arg) >>= ret);
        return ret;
      } catch(error& ) { }

      try {
        std::clog << "opening arg/func" << std::endl;        
        s->unify(open(s, func), open(s, arg) >>= ret);
        return ret;
      } catch(error& ) { }
      
      std::clog << "nope :/" << std::endl;        
      throw;
    }
    
    
  }


  // non-recursive let
  static mono infer(const ref<state>& s, const let& self) {
    auto sub = scope(s);

    for(ast::bind def : self.defs) {
      sub->vars->locals.emplace(def.id.name, s->generalize(infer(s, def.value)));
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
    
    const mono row = ext(self.id.name)(head)(tail);
    return record(row) >>= head;
  }


  // inj
  static mono infer(const ref<state>& s, const ast::inj& self) {
    const mono head = s->fresh(kind::term());
    const mono tail = s->fresh(kind::row());
    
    const mono row = ext(self.id.name)(head)(tail);
    return head >>= sum(row);
  }
  
  
  // record attrs
  static mono infer(const ref<state>& s, const list<ast::record::attr>& attrs) {
    const mono init = empty;
    return foldr(init, attrs, [&](ast::record::attr attr, mono tail) {
        return ext(attr.id.name)(infer(s, attr.value))(tail);
      });
  }

  
  // record
  static mono infer(const ref<state>& s, const ast::record& self) {
    return record(infer(s, self.attrs));
  }


  // match
  static mono infer(const ref<state>& s, const ast::match& self) {
    // match result type
    const mono result = s->fresh();

    const mono tail = s->fresh(kind::row());
    
    const mono rows =
      foldr(tail, self.cases,
            [&](ast::match::handler h, mono tail) {
              // build a lambda for handler body
              const ast::abs lambda =
              {h.arg >>= ast::abs::arg::list(), h.value};
              
              // unify lambda type
              const mono from = s->fresh();
              s->unify(from >>= result, infer(s, lambda));
              
              return row(h.id.name, from) |= tail;
            });

    if(self.fallback) {
      // handle fallback result
      s->unify(result, infer(s, *self.fallback));
    } else {
      // seal sum type: exclude any sum term not in the ones given by the match
      s->unify(tail, empty);
    }
    
    // final match type
    return sum(rows) >>= result;
  }
  


  // make
  static mono infer(const ref<state>& s, const ast::make& self) {
    // get reified constructor
    const mono reified = infer(s, *self.type);

    const mono type = reconstruct(s, reified);
    // std::clog << "reified: " << s->generalize(reified) << std::endl
    //           << "type: " << s->generalize(type) << std::endl;
    
    // obtain actual constructor
    const cst ctor = constructor(s, type);

    // module signature
    const poly sig = signature(s, ctor);
    // std::clog << "signature: " << sig << std::endl;
      
    auto sub = scope(s);
    
    const mono outer = s->fresh();
    const mono inner = sub->fresh();

    // instantiate signature at sub level prevents generalization of
    // contravariant side (variables only appearing in the covariant side will
    // be generalized)
    s->unify(outer >>= inner, sub->instantiate(sig));
    
    // vanilla covariant type
    const poly reference = sub->generalize(inner);
    
    // build provided type
    const mono init = empty;
    const mono provided =
      record(foldr(init, self.attrs,
                   [&](const ast::record::attr attr, mono tail) {
                     return row(attr.id.name, infer(sub, attr.value)) |= tail;
                   }));

    // std::clog << "inner: " << s->generalize(inner) << std::endl;
    // std::clog << "provided: " << s->generalize(provided) << std::endl;    
    
    // now also unify inner with provided type
    s->unify(inner, provided);

    // now generalize the provided type
    const poly gen = sub->generalize(inner);

    // generalization check: quantified variables in reference/gen should
    // substitute to the same variables
    std::set<var> quantified;
    for(const var& v : gen.forall) {
      assert(sub->sub->substitute(v) == v);
      quantified.insert(v);
    }

    // make sure all reference quantified references substitute to quantified
    // variables
    for(const var& v : reference.forall) {
      const mono vs = sub->sub->substitute(v);
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
      infer(s, ast::let(ast::bind{self.id, *self.value} >>= list<ast::bind>(),
                        self.id));
    try {
      s->def(self.id.name, value);
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
    using ::unit;
    foldr_rows(unit(), s->sub->substitute(row), [&](symbol attr, mono t, unit) {
        // TODO generalization issue?
        sub->def(attr, t);
        return unit();
      });
    
    return infer(sub, *self.body);
  }


  // import
  static mono infer(const ref<state>& s, const ast::import& self) {
    auto it = s->vars->locals.find(self.package);
    if(it != s->vars->locals.end()) {
      // TODO delegate to def
      throw error("variable " + tool::quote(self.package.get()) + " already defined");
    }
    
    const package pkg = package::import(self.package);
    
    s->vars->locals.emplace(self.package, pkg.sig());
    return io(unit);
  }


  // infer kind from reified type
  static kind::any infer_kind(const ref<state>& s, mono self) {
    return foldr_args(s, kind::term(), self, [&](mono arg, kind::any k) {
      return infer_kind(s, open(s, s->sub->substitute(arg))) >>= k;
    });
  }


  static mono reify(const ref<state>& s, mono self) {
    if(self.kind() == kind::term()) return ty(self);
    
    if(self.kind() == (kind::term() >>= kind::term())) {
      const mono a = s->fresh();
      return ty(a) >>= ty(self(a));
    }

    throw std::logic_error("reify not implemented");
    // * => type(self)
    // (* -> *) => type a -> type (self a)
  }


  

  // module
  static mono infer(const ref<state>& s, const ast::module& self) {

    // module scope
    const auto sub = function_scope(s, self.args);

    // argument types
    const list<mono> args = map(self.args, [&](auto arg) {
      return s->instantiate(sub->vars->find(arg.name()));      
    });

    // build module signature type
    const mono result = s->fresh();
    
    const mono sig = foldr(result, args, [&](mono arg, mono rhs) {
      return arg >>= rhs;
    });

    // infer attribute types in module scope
    const mono attrs = infer(sub, self.attrs);

    // recover inner type from reified types
    const mono inner =
      record(foldr_rows(empty, attrs, [&](symbol name, mono reified, mono rhs) {
        return row(name, reconstruct(sub, reified)) |= rhs;
      }));
    
    // infer kind from signature
    const kind::any k = infer_kind(s, sig);

    // create a new type constructor with given name/computed kind    
    const cst c = make_ref<constant>(self.id.name, k);

    // apply constructor to the right type variables
    const mono outer = foldl_kind(mono(c), k, [&](mono lhs, kind::any k) {
      return lhs(s->fresh(k));
    });

    // unify properly kinded variables to the right input variables
    foldr_args(s, outer, sig, [&](mono reified, mono rhs) {
      return rhs.match([&](app self) {
        // open reified type, and match against reified ctor arg
        s->unify(open(s, reified), reify(s, self->arg));

        // continue peeling constructor args
        return self->ctor;
      }, [&](mono self) -> mono { throw error("derp"); });
    });

    // unify result type with outer type
    s->unify(ty(outer), result);

    // define signature
    auto it = s->sigs->emplace(c, s->generalize(outer >>= inner));
    (void) it;

    // define constructor
    s->def(self.id.name, sig);

    // return s->instantiate(it.first->second);
    return io(unit);
  }


  
  // lit
  static mono infer(const ref<state>&, const ast::lit<::unit>& self) {
    return unit;
  }
  
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
    try {
      return self.visit(infer_visitor(), s);
    } catch(error& e) {
      std::stringstream ss;
      ss << "when processing: " << self;
      std::throw_with_nested(error(ss.str()));
    }
  }

  
}
