#include "repr.hpp"

#include "ast.hpp"
#include "tool.hpp"

namespace ast {

  ////////////////////////////////////////////////////////////////////////////////
 namespace {
    struct repr_visitor {

      template<class T>
      sexpr operator()(const T& self) const {
        return repr(self);
      }
    
    };
  }
  
  template<class T>
  static sexpr repr(const T& ) {
    static_assert(sizeof(T) == 0, "unimplemented repr");
    // throw std::logic_error("unimplemented repr: " + tool::type_name(typeid(T)));
  }

  template<class T>
  static sexpr repr(const lit<T>& self) {
    return self.value;
  }

  static sexpr repr(const lit<unit>& ) {
    return sexpr::list();
  }


  static sexpr repr(const var& self) {
    return self.name;
  }

  static sexpr repr(const abs::typed& self) {
    return repr(self.type) >>= self.id.name >>= sexpr::list();
  }

  static sexpr repr(const abs::arg& self) {
    return self.visit(repr_visitor());
  }

  static sexpr repr(const abs& self) {
    return kw::abs
      >>= map(self.args, [](abs::arg arg) -> sexpr {
          return repr(arg);
        })
      >>= repr(*self.body)
      >>= sexpr::list();
  }


  static sexpr repr(const app& self) {
    return 
      repr(*self.func)
      >>= map(self.args, [](const expr& e) {
          return repr(e);
      });
  }


  static sexpr repr(const let& self) {
    return kw::let
      >>= map(self.defs, [](const bind& self) -> sexpr {
          return self.id.name >>= repr(self.value) >>= sexpr::list();
        })
      >>= repr(*self.body)
      >>= sexpr::list();
  }


  static sexpr repr(const io& self) {
    return self.visit(repr_visitor());
  }

  
  static sexpr repr(const seq& self) {
    return kw::seq
      >>= map(self.items, repr_visitor())
      >>= sexpr::list();
  }


  static sexpr repr(const def& self) {
    return kw::def
      >>= self.id.name
      >>= repr(*self.value)
      >>= sexpr::list();
  }

    


  static sexpr repr(const sel& self) {
    return symbol(std::string(1, selection_prefix) + self.id.name.get());
  }

  static sexpr repr(const inj& self) {
    return symbol(std::string(1, injection_prefix) + self.id.name.get());
  }
  

  static sexpr repr(const record& self) {
    return kw::record
      >>= map(self.attrs, [](record::attr attr) -> sexpr {
          return attr.id.name >>= repr(attr.value) >>= sexpr::list();
        });
  }


  static sexpr repr(const bind& self) {
    return kw::bind
      >>= self.id.name
      >>= repr(self.value)
      >>= sexpr::list();
  }
  
  static sexpr repr(const cond& self) {
    return kw::cond
      >>= repr(*self.test)
      >>= repr(*self.conseq)
      >>= repr(*self.alt)
      >>= sexpr::list();
  }


  static sexpr repr(const make& self) {
    return kw::make
      >>= repr(*self.type)
      >>= map(self.attrs, [](record::attr attr) -> sexpr {
          return attr.id.name >>= repr(attr.value) >>= sexpr::list();
        });
  }


  static sexpr repr(const module& self) {
    return kw::module
      >>= sexpr(self.id.name >>= map(self.args, repr_visitor()))
      >>= map(self.attrs, [](record::attr attr) -> sexpr {
          return attr.id.name >>= repr(attr.value) >>= sexpr::list();
        });
  }
  
  static sexpr repr(const use& self) {
    return kw::use
      >>= repr(*self.env)
      >>= repr(*self.body)
      >>= sexpr::list();
  }

  static sexpr repr(const import& self) {
    return kw::import
      >>= self.package
      >>= sexpr::list();
  }

  static sexpr repr(const match& self) {
    return kw::match
      >>= map(self.cases, [](match::handler self) -> sexpr {
        return self.id.name >>= repr(self.arg) >>= repr(self.value) >>= sexpr::list();
        });
  }
      

  sexpr repr(const expr& self) {
    return self.visit(repr_visitor());
  }
  

  

}
