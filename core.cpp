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


  const mono list = make_ref<type::constant>("list", kind::term() >>= kind::term());
  
  {
    const auto a = self.ts->fresh();
    self.def("nil", list(a), value::list());
  }

  {
    const auto a = self.ts->fresh();      
    self.def("cons", a >>= list(a) >>= list(a),
             closure(2, [](const value* args) -> value {
               return args[0] >>= args[1].cast<value::list>();
             }));
  }

  {
    const auto a = self.ts->fresh();      
    self.def("isnil", list(a) >>= type::boolean,
             closure(+[](const value::list& self) -> boolean {
               return !boolean(self);
             }));
  }

  {
    const auto a = self.ts->fresh();
    self.def("head", list(a) >>= a,
             closure(1, [](const value* args) -> value {
               return args[0].cast<value::list>()->head;
             }));
  }

  {
    const auto a = self.ts->fresh();      
    self.def("tail", list(a) >>= list(a),
             closure(1, [](const value* args) -> value {
               return args[0].cast<value::list>()->tail;
             }));
  }


  using type::ty;
  auto ctor_value = closure(+[](const unit&) { return unit();});
  auto ctor2_value = closure(+[](const unit&, const unit& ) { return unit();});
  
  
  {
    const mono a = self.ts->fresh(kind::term());
    self.def("list", ty(a) >>= ty(list(a)), ctor_value);
  }

  {
    const mono a = self.ts->fresh(kind::term());
    const mono sig = list(a) >>= list(a);
    self.ts->sigs->emplace(*list.get<type::cst>(), self.ts->generalize(sig));
  }

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
  
