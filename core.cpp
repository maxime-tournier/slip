#include "package.hpp"

package package::core() {
  package self;
  
  using namespace eval;
  
  using type::list;
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

  {
    const auto a = self.ts->fresh();
    self.def("nil", type::list(a), value::list());
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
  self.def("list", ty >>= ty, unit());

  // TODO make this infix for great justice
  self.def("->", ty >>= ty >>= ty, unit());

  {
    self.def("functor", (ty >>= ty) >>= ty, unit());

    mono f = self.ts->fresh(kind::term() >>= kind::term());
    mono a = self.ts->fresh(kind::term());
    mono b = self.ts->fresh(kind::term());    

    mono functor = make_ref<type::constant>("functor", f.kind() >>= kind::term());

    mono t = functor(f) >>=
      type::record(row("map", a >>= b >>= f(a) >>= f(b)) |= type::empty);
    
    self.ts->sigs->locals.emplace("functor", self.ts->generalize(t));
  }

  
  return self;
}
  
