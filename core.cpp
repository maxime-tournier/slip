#include "package.hpp"

package package::core() {
  package self;
  
  using namespace eval;
  
  using type::real;
    
  using type::record;
  using type::row;
  using type::empty;
  using type::mono;
  using type::constant;

  // terms
  self
    .def("+", type::integer >>= type::integer >>= type::integer,
         closure(+[](const integer& lhs, const integer& rhs) -> integer {
           return lhs + rhs;
         }))
      
    .def("*", type::integer >>= type::integer >>= type::integer,
         closure(+[](const integer& lhs, const integer& rhs) -> integer {
           return lhs * rhs;
         }))
      
    .def("-", type::integer >>= type::integer >>= type::integer,
         closure(+[](const integer& lhs, const integer& rhs) -> integer {
           return lhs - rhs;
         }))
      
    .def("=", type::integer >>= type::integer >>= type::boolean,
         closure(+[](const integer& lhs, const integer& rhs) -> boolean {
           return lhs == rhs;
         }))
    ;


  using type::ty;
  auto ctor_value = closure(+[](const unit&) { return unit();});
  auto ctor2_value = closure(+[](const unit&, const unit& ) { return unit();});
  
  // function
  {
    const mono a = self.ts->fresh(kind::term());
    const mono b = self.ts->fresh(kind::term());    
    self.def("->", ty(a) >>= ty(b) >>= ty(a >>= b), ctor2_value);
  }

  {
    const mono a = self.ts->fresh(kind::term());
    const mono b = self.ts->fresh(kind::term());    

    const mono sig = (a >>= b) >>= (a >>= b);
    self.ts->sigs->emplace(*type::func.get<type::cst>(),
                           self.ts->generalize(sig));
  }


  {
    const mono a = self.ts->fresh(kind::term());
    const mono sig = ty(a) >>= ty(a);

    self.ts->sigs->emplace(*type::ty.get<type::cst>(),
                           self.ts->generalize(sig));
  }


  {
    const mono a = self.ts->fresh(kind::term());

    self.def("type", ty(a) >>= ty(ty(a)), ctor_value);

  }

  const mono ctor =
    make_ref<type::constant>("ctor",
                             (kind::term() >>= kind::term()) >>= kind::term());
  
  {
    const mono c = self.ts->fresh(kind::term() >>= kind::term());
    const mono a = self.ts->fresh(kind::term());

    const mono sig = ctor(c) >>= (ty(a) >>= ty(c(a)));
    
    self.ts->sigs->emplace(*ctor.get<type::cst>(), self.ts->generalize(sig));
  }

  {
    const mono c = self.ts->fresh(kind::term() >>= kind::term());    
    self.def("ctor", ctor(c) >>= ty(ctor(c)), ctor_value);
  }

  self.def("integer", ty(type::integer), unit());
  self.def("boolean", ty(type::boolean), unit());



  // maybe
  {
    const mono maybe = make_ref<type::constant>("maybe", kind::term() >>= kind::term());
    
    {
      const mono a = self.ts->fresh();
      const mono sig = maybe(a) >>=
        type::sum(row("some", a)
                  |= row("none", type::unit)
                  |= type::empty);
      
      self.ts->sigs->emplace(maybe.cast<type::cst>(), self.ts->generalize(sig));
    }

    {
      const mono a = self.ts->fresh();
      self.def("none", maybe(a), eval::sum{"none", unit()});
    }

    {
      const mono a = self.ts->fresh();
      self.def("just", a >>= maybe(a), eval::closure(1, [](const eval::value* args) -> value{
            return eval::sum("some", args[0]);
          }));
    }

    {
      const mono a = self.ts->fresh();
      self.def("maybe", ty(a) >>= ty(maybe(a)), ctor_value);
    }
    
  }

  // list
  {
    const mono list = make_ref<type::constant>("list", kind::term() >>= kind::term());
    
    {
      const mono a = self.ts->fresh();
      const mono sig = list(a) >>=
        type::sum(row(eval::cons, type::record(row(eval::head, a) |= row(eval::tail, list(a)) |= type::empty))
                  |= row(eval::nil, type::unit)
                  |= type::empty);
      
      self.ts->sigs->emplace(list.cast<type::cst>(), self.ts->generalize(sig));
    }
    
    {
      const mono a = self.ts->fresh();
      self.def("nil", list(a), eval::value::list());
    }

    {
      const mono a = self.ts->fresh();
      self.def("cons", a >>= list(a) >>= list(a), eval::closure(2, [](const eval::value* args) -> value {
            return args[0] >>= args[1].cast<eval::value::list>();
          }));
    }

    {
      const mono a = self.ts->fresh();
      self.def("list", ty(a) >>= ty(list(a)), ctor_value);
    }
    
  }


  
  // {
    
  //   self.def("functor", (ty >>= ty) >>= ty, unit());
    
  //   const mono f = self.ts->fresh(kind::term() >>= kind::term());
  //   const mono a = self.ts->fresh(kind::term());
  //   const mono b = self.ts->fresh(kind::term());    

  //   const mono functor =
  //     make_ref<type::constant>("functor", f.kind() >>= kind::term());

  //   const mono sig = functor(f) >>=
  //     type::record(row("map", a >>= b >>= f(a) >>= f(b)) |= type::empty);
    
  //   self.ts->sigs->emplace(*functor.get<type::cst>(), self.ts->generalize(sig));
  // }

  
  return self;
}
  
