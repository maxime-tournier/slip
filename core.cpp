#include "core.hpp"

namespace core {

  void setup(ref<type::state> s, ref<eval::state> e) {
    {
      using namespace eval;
      (*e)
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
    
        .def("nil", value::list())
        .def("cons", closure(2, [](const value* args) -> value {
          return args[0] >>= args[1].cast<value::list>();
        }))
    
        .def("isnil", closure(+[](const value::list& self) -> boolean {
          return !boolean(self);
        }))
        .def("head", closure(1, [](const value* args) -> value {
          return args[0].cast<value::list>()->head;
        }))
        .def("tail", closure(1, [](const value* args) -> value {
          return args[0].cast<value::list>()->tail;
        }))

        .def("functor", unit())
        .def("monad", unit())    

        ;
    }

    {
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

}


  
