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
  {
    self.def("list", ty >>= ty, unit());
    self.def("->", ty >>= ty >>= ty, unit());
  }
  

  return self;
}
  
