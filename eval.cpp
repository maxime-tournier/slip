#include "eval.hpp"

#include <vector>

#include "sexpr.hpp"
#include "package.hpp"
#include "gc.hpp"

namespace eval {

  ////////////////////////////////////////////////////////////////////////////////
  
  const symbol cons = "cons";
  const symbol nil = "nil";    

  const symbol head = "head";
  const symbol tail = "tail";    


  template<class T>
  static value eval(state* env, const T& ) {
    static_assert(sizeof(T) == 0, "eval implemented");
  }
  
  closure::closure(std::size_t argc, func_type func)
    : func(func),
      argc(argc) {

  }

  sum::sum(const value& data, symbol tag)
    : value(data), tag(tag) { }


  lambda::lambda(std::size_t argc, func_type func, state* env)
    : closure(argc, func),
      env(env) { }
      
  
  state::state(state* parent) : parent(parent) { }

  state* scope(state* self) {
    assert(self);
    return new state(self);
  }


  void state::def(symbol name, const value& self) {
    auto err = locals.emplace(name, self); (void) err;
    assert(err.second && "redefined variable");
  }

  
  template<class NameIterator, class ValueIterator>
  static state* augment(state* self,
                        NameIterator name_first, NameIterator name_last,
                        ValueIterator value_first, ValueIterator value_last) {
    auto res = scope(self);

    ValueIterator value = value_first;
    for(NameIterator name = name_first; name != name_last; ++name) {
      assert(value != value_last && "not enough values");
      res->locals.emplace(*name, *value++);
    }

    assert(value == value_last && "too many values");

    return res;
  }
  
  
  // apply a lambda to argument range
  value apply(const value& self, const value* first, const value* last) {
    const std::size_t argc = last - first;
    
    const std::size_t expected = self.match([&](const value& ) -> std::size_t {
        throw std::runtime_error("type error in application");
      },
      [&](const closure& self) { return self.argc; });
    
    if(argc < expected) {
      // unsaturated call: build wrapper
      const std::size_t remaining = expected - argc;        
      const std::vector<value> saved(first, last);
    
      return closure(remaining, [self, saved, remaining](const value* args) {
          std::vector<value> tmp = saved;
          for(auto it = args, last = args + remaining; it != last; ++it) {
            tmp.emplace_back(*it);
          }
          return apply(self, tmp.data(), tmp.data() + tmp.size());
        });
    }

    if(argc > expected) {
      // over-saturated call: call result with remaining args
      const value* mid = first + expected;
      assert(mid > first);
      assert(mid < last);
      
      const value func = apply(self, first, mid);
      return apply(func, mid, last);
    }

    // saturated calls
    return self.match([&](const value& ) -> value {
        throw std::runtime_error("type error in application");
      },
      [&](const closure& self) {
        return self.func(first);
      });
  }

  

  template<class T>
  static value eval(state*, const ast::lit<T>& self) {
    return self.value;
  }

  static value eval(state*, const ast::lit<string>& self) {
    return make_ref<string>(self.value);
  }


  static value eval(state* e, const ast::var& self) {
    auto res = e->find(self.name); assert(res);
    return *res;
  }

  
  static value eval(state* e, const ast::app& self) {
    // note: evaluate func first
    const value func = eval(e, *self.func);
    
    std::vector<value> args;
    foldl(unit(), self.args, [&](unit, const ast::expr& self) {
        args.emplace_back(eval(e, self));
        return unit();
      });
    
    return apply(func, args.data(), args.data() + args.size());
  }

  
  static value eval(state* e, const ast::abs& self) {
    std::vector<symbol> names;
    
    for(const auto& arg : self.args) {
      names.emplace_back(arg.name());
    }

    const ast::expr body = *self.body;
    
    return lambda(names.size(), [=](const value* args) {
      auto sub = augment(e, names.begin(), names.end(),
                         args, args + names.size());
      return eval(sub, body);
    }, e);
  }


  static value eval(state* e, const ast::bind& self) {
    auto it = e->locals.emplace(self.id.name, eval(e, self.value)); (void) it;
    assert(it.second && "redefined variable");
    return unit();
  }
  
  
  static value eval(state* e, const ast::io& self) {
    return self.match([&](const ast::expr& self) {
        return eval(e, self);
      },
      [&](const ast::bind& self) {
        return eval(e, self);
      });
  }

  

  static value eval(state* e, const ast::seq& self) {
    auto sub = scope(e);
    
    const value init = unit();
    foldl(init, self.items, [&](const value&, const ast::io& self) {
        return eval(sub, self);
      });

    // return
    return eval(sub, *self.last);
  }


  static value eval(state* e, const ast::run& self) {
    return eval(e, *self.value);
  }

  static value eval(state* e, const ast::module& self) {
    // just define the reified module type constructor
    enum module::type type;
    switch(self.type) {
    case ast::module::product: type = module::product; break;
    case ast::module::coproduct: type = module::coproduct; break;      
    }
    return module{type};
  }

  
  
  static value eval(state* e, const ast::def& self) {
    auto it = e->locals.emplace(self.id.name, eval(e, *self.value)); (void) it;
    assert(it.second && "redefined variable");
    return unit();
  }
  
  
  static value eval(state* e, const ast::let& self) {
    auto sub = scope(e);
    
    for(const ast::bind& def : self.defs) {
      sub->locals.emplace(def.id.name, eval(sub, def.value));
    }

    return eval(sub, *self.body);
  }

  
  static value eval(state* e, const ast::cond& self) {
    const value test = eval(e, *self.test);
    assert(test.get<boolean>() && "type error");
    
    if(test.cast<boolean>()) return eval(e, *self.conseq);
    else return eval(e, *self.alt);
  }

				   


  static value eval(state* e, const ast::record& self) {
    auto res = make_ref<record>();
    for(const auto& attr : self.attrs) {
      res->emplace(attr.id.name, eval(e, attr.value));
    }
    return res;
  }


  static value eval(state* e, const ast::sel& self) {
    const symbol name = self.id.name;
    return closure(1, [name](const value* args) -> value {
        return args[0].match([&](const value::list& self) -> value {
            // note: the only possible way to call this is during a pattern
            // match processing a non-empty list
            assert(self && "type error");
            if(name == head) return self->head;
            if(name == tail) return self->tail;
            assert(false && "type error");
          },
          [&](const ref<record>& self) {
            const auto it = self->find(name); (void) it;
            assert(it != self->end() && "type error");
            return it->second;
          },
          [&](const value& self) -> value {
            assert(false && "type error");
          });
      });
  }


  static value eval(state* e, const ast::inj& self) {
    const symbol tag = self.id.name;
    return closure(1, [tag](const value* args) -> value {
        return make_ref<sum>(args[0], tag);
    });
  }
  

  static value eval(state* e, const ast::make& self) {
    switch(eval(e, *self.type).cast<module>().type) {
    case module::product:
      return eval(e, ast::record{self.attrs});
    case module::coproduct: {
      assert(size(self.attrs) == 1);
      const auto& attr = self.attrs->head;
      return make_ref<sum>(eval(e, attr.value), attr.id.name);
    }
    case module::list:
      assert(size(self.attrs) == 1);      
      return eval(e, self.attrs->head.value);
    };
  }

  
  static value eval(state* e, const ast::use& self) {
    const value env = eval(e, *self.env);
    assert(env.get<ref<record>>() && "type error");

    // auto s = scope(e);
    
    for(const auto& it : *env.cast<ref<record>>()) {
      e->def(it.first, it.second);
    }

    // return eval(s, *self.body);
    return unit();
  }
  

  static value eval(state* e, const ast::import& self) {
    const package pkg = package::import(self.package);
    e->def(self.package, pkg.dict());
    return unit();
  }



  static value eval(state* e, const ast::match& self) {
    std::map<symbol, std::pair<symbol, ast::expr>> dispatch;

    for(const auto& handler : self.cases) {
      auto err = dispatch.emplace(handler.id.name,
                                  std::make_pair(handler.arg.name(),
                                                 handler.value));
      (void) err; assert(err.second);
    }
    
    const ref<ast::expr> fallback = self.fallback;

    return closure(1, [e, dispatch, fallback](const value* args) {
        // matching on a list
        return args[0].match([&](const value::list& self) {
            auto it = dispatch.find(self ? cons : nil);
            if(it != dispatch.end()) {
              auto sub = scope(e);
              sub->def(it->second.first, self);
              return eval(sub, it->second.second);
            } else {
              assert(fallback);
              return eval(e, *fallback);
            }
          },
          [&](const ref<sum>& self) {
            auto it = dispatch.find(self->tag);
            if(it != dispatch.end()) {
              auto sub = scope(e);
              sub->def(it->second.first, *self);
              return eval(sub, it->second.second);
            } else {
              assert(fallback);
              return eval(e, *fallback);
            }
          },
          [&](const value& self) -> value {
            std::stringstream ss;
            ss << "attempting to match on value " << self;
            throw std::runtime_error(ss.str());
          });
      });
  }

  namespace {
    struct eval_visitor {
  
      template<class T>
      value operator()(const T& self, state* e) const {
        return eval(e, self);
      }
  
    };
  }

  value eval(state* e, const ast::expr& self) {
    return self.visit(eval_visitor(), e);
  }

  
namespace {
  
struct ostream_visitor {
  
  void operator()(const ref<value>& self, std::ostream& out) const {
    out << "#mut<" << *self << ">";
  }
  
  void operator()(const module& self, std::ostream& out) const {
    out << "#<module>";
  }


  void operator()(const ref<string>& self, std::ostream& out) const {
    out << '"' << *self << '"';
  }
  
  void operator()(const symbol& self, std::ostream& out) const {
    out << self;
  }

  
  void operator()(const value::list& self, std::ostream& out) const {
    out << self;
  }


  void operator()(const lambda& self, std::ostream& out) const {
	out << "#<lambda>";
  }
  
  
  void operator()(const closure& self, std::ostream& out) const {
	out << "#<closure>";
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

  
  void operator()(const ref<record>& self, std::ostream& out) const {
    out << "{";
    bool first = true;
    for(const auto& it : *self) {
      if(first) first = false;
      else out << "; ";
      out << it.first << ": " << it.second;
    }
    out << "}";
  }

  
  void operator()(const ref<sum>& self, std::ostream& out) const {
    out << "<" << self->tag << ": " << *self << ">";
  }
  
};
}

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.visit(ostream_visitor(), out);
    return out;
  }


  static void mark(const value& self, bool debug) {
    self.match([&](const value& ) { },
               [&](const ref<record>& self) {
                 for(const auto& it : *self) {
                   mark(it.second, debug);
                 }
               },
               [&](const ref<sum>& self) {
                 mark(*self, debug);
               },
               [&](const lambda& self) {
                 mark(self.env, debug);
               });
  }
  

  void mark(state* e, bool debug) {
    if(debug) std::clog << "marking:\t" << e << std::endl;
    
    if(e->marked()) return;
    e->mark();
    
    for(auto& it : e->locals) {
      mark(it.second, debug);
    }

    if(e->parent) {
      mark(e->parent, debug);
    }
  }

  
  value* state::find(symbol name)  {
    auto it = locals.find(name);
    if(it != locals.end()) return &it->second;
    if(parent) return parent->find(name);
    return nullptr;
  }
  
}
