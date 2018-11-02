#ifndef SLAP_TYPE_HPP
#define SLAP_TYPE_HPP

#include <set>
#include <functional>

#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "environment.hpp"

#include "kind.hpp"



namespace type {

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  struct unification_error : error {
    using error::error;
  };

  struct logger;
  
  struct mono;
  struct poly;

  struct constant;
  struct variable;
  struct application;

  // monotypes
  using cst = ref<constant>;
  using var = ref<variable>;
  using app = ref<application>;

  
  struct mono : variant<cst, var, app> {
    using mono::variant::variant;

    ::kind::any kind() const;
  
    // convenience constructor for applications
    mono operator()(mono arg) const;
  };


  

  struct constant {
    const symbol name;
    const ::kind::any kind;
    
    constant(symbol name, ::kind::any k);
  };


  struct variable {
    const std::size_t level;
    const ::kind::any kind;

    variable(std::size_t level, const ::kind::any kind);
  };


  struct application {
    const mono ctor;
    const mono arg;

    application(mono ctor, mono arg);
  };


  // polytypes
  struct poly {
    using forall_type = std::set<var>;
    const forall_type forall;
    const mono type;

    friend std::ostream& operator<<(std::ostream& out, const poly& self);
  };


  // constructors for literals
  const extern mono unit, boolean, integer, real, string;

  // higher kinded constructors
  const extern mono func;
  const extern mono io;
  
  const extern mono record, sum, empty;

  // reified type constructor
  const extern mono ty;
  
  mono ext(symbol attr);

  // convenience: build function types
  mono operator>>=(mono lhs, mono rhs);

  
  // convenience: build row types
  struct row {
    const symbol attr;
    const mono head;

    row(symbol attr, mono head) : attr(attr), head(head) { }

    mono operator|=(mono tail) const {
      return ext(attr)(head)(tail);
    }
  };

  
  // convenience: unpack record types
  struct extension {
    const symbol attr;
    const mono head;
    const mono tail;
    
    static extension unpack(const app& self);
  };  

  // debugging helper
  struct logger {
    std::ostream& out;

    struct pimpl_type;
    std::unique_ptr<pimpl_type> pimpl;

    logger(std::ostream& out);
    ~logger();
    
    template<class T>
    logger& operator<<(const T& x) {
      out << x;
      return *this;
    }

    logger& operator<<(std::ostream& (*)(std::ostream&));
    logger& operator<<(const poly& p);
  };
}



#endif
