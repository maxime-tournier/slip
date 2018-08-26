#include "core.hpp"

namespace core {

  void setup(package& self) {

    using namespace eval;
    using type::list;
    using type::real;
    
    using type::module;
    using type::record;
    
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

    // self.def("functor", unit());
    // self.def("monad", unit())    

      ;
  }
  
  void setup(ref<type::state> s) {
    using namespace type;
    using type::integer;
    using type::boolean;
    using type::list;
    using type::real;
    
    using type::module;
    using type::record;
    
    // arithmetic
    (*s)
      .def("+", integer >>= integer >>= integer)
      .def("*", integer >>= integer >>= integer)      
      .def("-", integer >>= integer >>= integer)
      .def("=", integer >>= integer >>= boolean);

    // lists
    {
      const auto a = s->fresh();
      s->def("nil", list(a));
    }

    {
      const auto a = s->fresh();
      s->def("cons", a >>= list(a) >>= list(a));
    }

    // 
    {
      const auto a = s->fresh();
      s->def("isnil", list(a) >>= boolean);
    }

    {
      const auto a = s->fresh();
      s->def("head", list(a) >>= a);
    }

    {
      const auto a = s->fresh();
      s->def("tail", list(a) >>= list(a));
    }
    
    
    // constructors
    (*s)
      .def("boolean", module(boolean)(boolean))
      .def("integer", module(integer)(integer))
      .def("real", module(real)(real))
      ;    

    {
      // functor
      const mono f = s->fresh(kind::term() >>= kind::term());

      const mono a = s->fresh(kind::term());
      const mono b = s->fresh(kind::term());      
      
      const mono functor
        = make_ref<constant>("functor", f.kind() >>= kind::term());
      
      s->def("functor", module(functor(f))
             (record(row("map", (a >>= b) >>= f(a) >>= f(b)) |= empty)))
        ;
    }

    {
      // monad
      const mono m = s->fresh(kind::term() >>= kind::term());

      const mono a = s->fresh(kind::term());
      const mono b = s->fresh(kind::term());
      const mono c = s->fresh(kind::term());            
      
      const mono monad
        = make_ref<constant>("monad", m.kind() >>= kind::term());
      
      s->def("monad", module(monad(m))
             (record(row("bind", m(a) >>= (a >>= m(b)) >>= m(b)) |=
                     row("pure", c >>= m(c)) |=
                     empty)))
        ;
    }
  }

}


  
