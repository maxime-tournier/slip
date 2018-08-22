#ifndef SLAP_TYPE_HPP
#define SLAP_TYPE_HPP

#include <set>

#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "environment.hpp"

#include "kind.hpp"

namespace ast {
struct expr;
};


namespace type {

struct error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct mono;
struct poly;

struct constant;
struct variable;
struct application;


// type state
struct state : environment<poly> {
  using substitution = std::map<ref<variable>, mono>;
  
  const std::size_t level;
  const ref<substitution> sub;

  state();
  state(const ref<state>& parent);

  ref<variable> fresh(kind::any k=kind::term()) const;
  
  mono substitute(const mono& t) const; 
  poly generalize(const mono& t) const;
  
  void unify(mono from, mono to);

  // convenience
  using environment<poly>::operator();

  // generalize t at current depth before inserting
  state& operator()(std::string, mono t);
};



// monotypes
using cst = ref<constant>;
using var = ref<variable>;
using app = ref<application>;

struct mono : variant<cst, var, app> {
  using mono::variant::variant;

  ::kind::any kind() const;
  
  static mono infer(const ref<state>& s, const ast::expr& self);

  // convenience constructor for applications
  mono operator()(mono arg) const;
};


struct constant {
  const symbol name;
  const ::kind::any kind;
  constant(symbol name, ::kind::any k) : name(name), kind(k) { }
};


struct variable {
  const std::size_t level;
  const ::kind::any kind;
};


struct application {
  const mono ctor;
  const mono arg;

  application(mono ctor, mono arg);
};


// polytypes
struct poly {
  using forall_type = std::set<ref<variable>>;
  const forall_type forall;
  const mono type;

  friend std::ostream& operator<<(std::ostream& out, const poly& self);
};


// constants
const extern mono unit, boolean, integer, real;

// constructors
  const extern mono func, list; // , io
  const extern mono rec, empty;

mono ext(symbol attr);

// convenience: build function types
mono operator>>=(mono lhs, mono rhs);




}



#endif
