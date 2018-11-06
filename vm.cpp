#include "vm.hpp"

#include "sexpr.hpp"
#include "package.hpp"

namespace vm {

  builtin::builtin(std::size_t argc, func_type func) {
    assert(argc < argc_mask);

    storage.func = func;
    // std::clog << "builtin: " << std::bitset<64>(storage.bits) << std::endl;
    storage.bits |= argc;
  }


  builtin::func_type builtin::func() const {
    auto res = storage;
    res.bits &= ~argc_mask;
    // std::clog << "func: " << std::bitset<64>(res.bits) << std::endl;
    return res.func;
  }

  std::size_t builtin::argc() const {
    std::size_t res = storage.bits & argc_mask;
    // std::clog << "argc: " << res << std::endl;
    return res;
  }

  
  state::state(std::size_t size):
    stack(size) {
    frames.reserve(size);    
    frames.emplace_back(stack.next(), nullptr);
  }

  record::record(std::map<symbol, value> attrs):
    attrs(attrs) {
    // std::clog << __func__ << " " << this << std::endl;
  }
  
  record::~record() {
    // std::clog <<  __func__ << " " << this << std::endl;
  }
  
  
  static void run(state* s, const ir::expr& self);  

  
  template<class T>
  static value run(state* s, const T& ) {
    throw std::runtime_error("run unimplemented for: "
                             + tool::type_name(typeid(T)));
  }

  
  static constexpr bool debug = false;


  static value* top(state* s) {
    return s->stack.next() - 1;
  }

  
  static void peek(state* s) {
    for(std::size_t i = 0; i < s->stack.size(); ++i) {
      std::clog << "\t" << i << ":\t" << *(s->stack.next() - (i + 1))<< std::endl;
    }
  }

  
  template<class ... Args>
  static value* push(state* s, Args&& ... args) {
    value* res = new (s->stack.allocate(1)) value(std::forward<Args>(args)...);
    // if(debug) std::clog << "\tpush:\t" << *res << std::endl;
    return res;
  }

  
  static void pop(state* s, std::size_t n) {
    // for(std::size_t i=0; i < n; ++i) {
    //   if(debug) std::clog << "\tpop:\t" << *(top(s) - i) << std::endl;
    //   (top(s) - i)->~value();
    // }
    
    s->stack.deallocate(s->stack.next() - n, n);
  }

  
  static value pop(state* s) {
    value res = std::move(*top(s));
    pop(s, 1);
    return res;
  }
  
  
  template<class T>
  static void run(state* s, const ir::lit<T>& self) {
    push(s, self.value);
  }


  static void run(state* s, const ir::lit<string>& self) {
    push(s, gc::make_ref<string>(self.value));
  }


  static void run(state* s, const ir::local& self) {
    assert(s->frames.back().sp);
    push(s, s->frames.back().sp[self.index]);
  }
  
  
  static void run(state* s, const ir::capture& self) {
    assert(s->frames.back().cp);    
    push(s, s->frames.back().cp[self.index]);
  }

  
  static void run(state* s, const ir::global& self) {
    auto it = s->globals.find(self.name);
    assert(it != s->globals.end());
    push(s, it->second);
  }

  
  static void run(state* s, const ir::block& self) {
    for(const ir::expr& e: self.items) {
      run(s, e);
    }
  }

  static void run(state* s, const ir::drop& self) {
    pop(s, self.count);
  }

  
  static void run(state* s, const ir::sel& self) {
    auto rec = pop(s).cast<gc::ref<record>>();
    auto it = rec->attrs.find(self.attr);
    assert(it != rec->attrs.end() && "record attribute error");
    push(s, it->second);
  }


  static void run(state* s, const ref<ir::match>& self) {
    // precondition: matched value is pushed and is a sum value

    const symbol tag = top(s)->cast<gc::ref<sum>>()->tag;
    auto it = self->cases.find(tag);

    if(it == self->cases.end()) {
      run(s, self->fallback);
    } else {
      run(s, it->second);
    }

    // TODO optimize?
    
    // pop match result
    value result = pop(s);

    // pop matched value
    pop(s, 1);

    // push back result
    push(s, std::move(result));
  }
  
  
  static void run(state* s, const ir::exit& self) {
    value result = pop(s);
    pop(s, self.locals);
    push(s, std::move(result));
  }
  

  static void run(state* s, const ref<ir::branch>& self) {
    // evaluate test
    const bool test = pop(s).cast<boolean>();

    if(test) {
      run(s, self->then);
    } else {
      run(s, self->alt);
    }
  }

  static value apply(state* s, const builtin& self,
                     const value* args, std::size_t argc);
  static value apply(state* s, const ref<closure>& self,
                     const value* args, std::size_t argc);
  static value apply(state* s, const value& self,
                     const value* args, std::size_t argc);  
  

  static value unsaturated(state* s, const value& self, std::size_t expected,
                           const value* args, std::size_t argc) {
    std::clog << "calling: " << self << " " << expected << " / " << argc << std::endl;
    throw std::runtime_error("unimplemented: unsaturated");
    
    assert(argc != expected);
    if(expected > argc) {
      // under-saturated: close over available arguments + function
      vector<value> captures(args, args + argc);
      captures.emplace_back(self);
      
      // closure call
      vector<ir::expr> items; items.reserve(1 + argc + expected);

      // push closure
      items.emplace_back(ir::capture(argc));
      
      // push argc leading args from closure
      for(std::size_t i = 0; i < argc; ++i) {
        items.emplace_back(ir::capture(i));
      }
      
      // push remaining args come from stack
      for(std::size_t i = 0, n = expected - argc; i < n; ++i) {
        items.emplace_back(ir::local(i));
      }

      // push saturated call
      items.emplace_back(ir::call{argc + expected});
      
      return gc::make_ref<closure>(expected - argc, ir::block{std::move(items)}, std::move(captures));
    } else {
      // over-saturated: call expected arguments then call remaining args
      // regularly
      value tmp = apply(s, self, args, expected);
      return tmp.match([&](const auto& self) {
          return apply(s, self, args + expected, argc - expected);
        });
    }    
  }
  


  // builtin call
  static value apply(state* s, const builtin& self,
                     const value* args, std::size_t argc) {
    if(self.argc() != argc) {
      return unsaturated(s, self, self.argc(), args, argc);
    }
    
    return self.func()(args);
  }

  // closure call
  static value apply(state* s, const gc::ref<closure>& self,
                     const value* args, std::size_t argc) {
    if(self->argc != argc) {
      return unsaturated(s, self, self->argc, args, argc);      
    }

    // push frame
    s->frames.emplace_back(args, self->captures.data());
    
    // evaluate stuff
    run(s, self->body);

    // pop result
    value result = pop(s);

    // sanity check
    assert(s->stack.next() - s->frames.back().sp == argc);
    
    // pop frame
    s->frames.pop_back();
    return result;
  }
  

  template<class T>
  static value apply(state* s, const T& self, const value* args, std::size_t argc) {
    throw std::runtime_error("type error in application: "
                             + tool::type_name(typeid(T)));
  }

  // main dispatch
  static value call(state* s, const value* args, std::size_t argc) {
    const value& self = args[-1];
    
    // switch(self.type()) {
    // case value::index_of<builtin>::value:
    //   return apply(s, self.cast<builtin>(), args, argc);
    // case value::index_of<gc::ref<closure>>::value:
    //   return apply(s, self.cast<gc::ref<closure>>(), args, argc);
    // default: break;
    // }
    
    // delegate call based on actual function type
    return self.match([&](const auto& self) {
        return apply(s, self, args, argc);
      });
  }


  static void run(state* s, const ir::call& self) {
    // arguments pointer
    const value* args = s->stack.next() - self.argc;
    
    // call function: call must cleanup the stack leaving exactly the function
    // and its arguments in place
    value result = call(s, args, self.argc);
    
    // pop arguments
    pop(s, self.argc);

    // push function result
    *top(s) = std::move(result);
  }


  static void run(state* s, const ref<ir::closure>& self) {
    // std::clog << "closure: " << repr(self) << std::endl;

    // result
    auto res = gc::make_ref<closure>(self->argc, self->body);

    // note: pushing closure *before* filling captures so that it can be
    // captured itself (recursive definitions)
    push(s, res);
    
    const value* first = s->stack.next();
    // push captured variables
    for(const ir::expr& c : self->captures) {
      run(s, c);
    }
    
    // TODO move values from the stack into vector
    std::vector<value> captures(first, first + self->captures.size());

    // pop captures
    pop(s, self->captures.size());

    // set result captures
    res->captures = std::move(captures);
  }

  static void run(state* s, const ir::import& self) {
    const state& pkg = package::import<state>(self.package, [&] {    
      state s;
      package::iter(self.package, [&](ast::expr self) {
        const ir::expr c = ir::compile(self);
        run(&s, c);
      });
      return s;
    });

    push(s, gc::make_ref<record>(pkg.globals));
  }

  
  static void run(state* s, const ir::def& self) {
    assert(s->frames.size() == 1 && "toplevel definition in local scope");

    s->def(self.name, pop(s));
    push(s, unit());
  }

  
  static void run(state* s, const ref<ir::use>& self) {
    assert(s->frames.size() == 1 && "non-toplevel use unimplemented");    

    // evalute use result
    run(s, self->env);

    value env = pop(s);
    
    // TODO compile local uses properly with type-system help
    for(const auto& it: env.cast<gc::ref<record>>()->attrs) {
      s->def(it.first, it.second);
    }

    push(s, unit());
  }


  static void run(state* s, const ir::record& self) {
    record::attrs_type attrs;
    
    for(auto it = self.attrs.rbegin(), end = self.attrs.rend(); it != end; ++it) {
      attrs.emplace(*it, pop(s));
    }

    push(s, gc::make_ref<record>(std::move(attrs)));
  }


  // main dispatch
  static void run(state* s, const ir::expr& self) {
    self.match([&](const auto& self) {
      run(s, self);
    });
  }

  ////////////////////////////////////////////////////////////////////////////////
  

  value eval(state* s, const ir::expr& self) {
    // std::clog << repr(self) << std::endl;
    run(s, self);
    value res = pop(s);

    // hack: prevent last value from being collected
    s->globals.emplace("__last__", res).first->second = res;
    
    collect(s);
    return res;
  }


  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.match([&](const auto& self) { out << self; },
               [&](const unit& self) { out << "()"; },
               [&](const gc::ref<closure>& ) { out << "#<closure>"; },
               [&](const builtin& ) { out << "#<builtin>"; },
               [&](const boolean& self) { out << (self ? "true" : "false"); },
               [&](const gc::ref<string>& self) { out << '"' << *self << '"';},
               [&](const gc::ref<record>& self) {
                 out << "{";
                 bool first=true;
                 for(const auto& it: self->attrs) {
                   if(first) first = false;
                   else out << "; ";
                   out << it.first << ": " << it.second;
                 }
                 out << "}";                 
               },
               [&](const gc::ref<sum>& self) {
                 out << "<" << self->tag << ": " << self->data << ">";
               });
    return out;
  }

  ////////////////////////////////////////////////////////////////////////////////
  struct mark_visitor {
    template<class T>
    void operator()(T self, bool debug) const { }

    template<class T>
    void operator()(gc::ref<T> self, bool debug) const {
      if(debug) std::clog << "  marking: " << self.get() << std::endl;              
      self.mark();
    }

    void operator()(gc::ref<record> self, bool debug) const {
      self.mark();
      
      for(auto& it : self->attrs) {
        if(debug) std::clog << "  visiting: " << it.first << std::endl;        
        it.second.visit(*this, debug);
      }
    }
    
  };
  
  static void mark(state* self, bool debug) {
    for(auto& it : self->globals) {
      if(debug) std::clog << "visiting: " << it.first << std::endl;
      it.second.visit(mark_visitor(), debug);
    }
  }

  void collect(state* self) {
    mark(self, false);
    gc::sweep();
  }
  
}
