#include "package.hpp"

#include "infer.hpp"
#include "eval.hpp"
#include "vm.hpp"

namespace kw {
static const symbol nil = "nil";
static const symbol cons = "cons";

static const symbol head = "head";
static const symbol tail = "tail";
}

namespace type {

  static ref<state> builtins() {
  
    ref<state> self = make_ref<state>();
  
    (*self)
      .def("+", integer >>= integer >>= integer)
      .def("*", integer >>= integer >>= integer)
      .def("-", integer >>= integer >>= integer)
      .def("=", integer >>= integer >>= boolean)
      ;

    // function
    {
      const mono a = self->fresh(kind::term());
      const mono b = self->fresh(kind::term());
    
      self->def("->", ty(a) >>= ty(b) >>= ty(a >>= b));
    }


    // type
    {
      const mono a = self->fresh(kind::term());
      self->def("type", ty(a) >>= ty(ty(a)));
    }
  
    // ctor
    const mono ctor = 
      make_ref<constant>("ctor",
                         (kind::term() >>= kind::term()) >>= kind::term());
  
    {
      const mono c = self->fresh(kind::term() >>= kind::term());
      const mono a = self->fresh(kind::term());

      const mono sig = ctor(c) >>= (ty(a) >>= ty(c(a)));
    
      self->sigs->emplace(ctor.cast<cst>(), self->generalize(sig));
    }
  
    {
      const mono c = self->fresh(kind::term() >>= kind::term());    
      self->def("ctor", ctor(c) >>= ty(ctor(c)));
    }

    // base types
    self->def("string", ty(string));    
    self->def("real", ty(real));    
    self->def("integer", ty(integer));
    self->def("boolean", ty(boolean));
    self->def("unit", ty(unit));
    
    // list
    const mono list = make_ref<constant>("list", kind::term() >>= kind::term());
    {
      const mono a = self->fresh();
      const mono sig = list(a) >>=
        sum(row(kw::cons, record(row(kw::head, a)
                                 |= row(kw::tail, list(a)) |= empty))
            |= row(kw::nil, unit)
            |= empty);
    
      self->sigs->emplace(list.cast<cst>(), self->generalize(sig));
    }
  
    {
      const mono a = self->fresh();
      self->def(kw::nil, list(a));
    }

    {
      const mono a = self->fresh();
      self->def(kw::cons, a >>= list(a) >>= list(a));
    }

    {
      const mono a = self->fresh();
      self->def("list", ty(a) >>= ty(list(a)));
    }

    //  io
    const mono world = make_ref<constant>("world", kind::term());
  
    const mono mut =
      make_ref<constant>("mut", kind::term() >>= kind::term() >>= kind::term());
  
    {
      const mono a = self->fresh();
      const mono b = self->fresh();
    
      self->def("ref", b >>= io(a)(mut(a)(b)));
    }

    {
      const mono a = self->fresh();
      const mono b = self->fresh();    

      self->def("get", mut(a)(b) >>= io(a)(b));
    }

    {
      const mono a = self->fresh();
      const mono b = self->fresh();    
 
      self->def("set", mut(a)(b) >>= b >>= io(a)(unit));
    }


    {
      const mono a = self->fresh();
      const mono t = self->fresh();
    
      self->def("pure", a >>= io(t)(a));
    }
  
  
    // strings
    self->def("print", string >>= io(world)(unit));

    return self;
  };
}

namespace eval {
  
  static state::ref builtins() {
    state::ref self = gc::make_ref<state>();

    (*self)
      .def("+", closure(+[](const integer& lhs, const integer& rhs) -> integer {
        return lhs + rhs;
      }))
    
      .def("*", closure(+[](const integer& lhs, const integer& rhs) -> integer {
        return lhs * rhs;
      }))
    
      .def("-", closure(+[](const integer& lhs, const integer& rhs) -> integer {
        return lhs - rhs;
      }))
    
      .def("=", closure(+[](const integer& lhs, const integer& rhs) -> boolean {
        return lhs == rhs;
      }))

      ;

    value ctor = closure(+[](const unit&) { return unit(); });
    value ctor2 = closure(+[](const unit&, const unit&) { return unit(); });    
    
    // ctors (will never be called)
    self->def("->", ctor2);
    self->def("type", ctor);
    self->def("ctor", ctor);    
    self->def("list", ctor);

    self->def("string", unit());        
    self->def("real", unit());    
    self->def("integer", unit());
    self->def("boolean", unit());
    self->def("unit", unit());

    
    // list
    self->def(kw::nil, eval::value::list());
  
    self->def(kw::cons, eval::closure(2, [](const eval::value* args) -> value {
      return args[0] >>= args[1].cast<eval::value::list>();
    }));



    // io
    self->def("ref", eval::closure(1, [](const eval::value* args) -> value {
      return make_ref<value>(args[0]);
    }));

    self->def("get", eval::closure(1, [](const eval::value* args) -> value {
      return *args[0].cast<ref<value>>();
    }));

    self->def("set", eval::closure(2, [](const eval::value* args) -> value {
      *args[0].cast<ref<value>>() = args[1];
      return unit();
    }));

    self->def("pure", eval::closure(1, [](const eval::value* args) {
      return args[0];
    }));
  
  
    // strings
    self->def("print", eval::closure(+[](const ref<string>& self) {
      std::cout << *self;
      return unit();
    }));
  
    return self;
  };
}


namespace vm {
  state builtins() {
    state self(1000);

    self
      .def("+", builtin([](const integer& lhs, const integer& rhs) -> integer {
        return lhs + rhs;
      }))
    
      .def("*", builtin([](const integer& lhs, const integer& rhs) -> integer {
        return lhs * rhs;
      }))
    
      .def("-", builtin([](const integer& lhs, const integer& rhs) -> integer {
        return lhs - rhs;
      }))
    
      .def("=", builtin([](const integer& lhs, const integer& rhs) -> boolean {
        return lhs == rhs;
      }))
      ;

    return self;
  }

}

namespace package {
  void builtins() {
    static const symbol name = "builtins";
    
    import<ref<type::state>>(name, type::builtins);
    
    import<eval::state::ref>(name, eval::builtins);

    import<vm::state>(name, vm::builtins);
  }
}

