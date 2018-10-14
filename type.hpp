#ifndef SLAP_TYPE_HPP
#define SLAP_TYPE_HPP

#include <set>
#include <functional>

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

// monotypes
using cst = ref<constant>;
using var = ref<variable>;
using app = ref<application>;


// type state
struct state {
  using vars_type = environment<poly>;
  using sigs_type = std::map<cst, poly>;
  
  const std::size_t level;
  
  const ref<vars_type> vars;
  const ref<sigs_type> sigs;
  
  using substitution = std::map<ref<variable>, mono>;
  
  const ref<substitution> sub;

  state();
  state(const ref<state>& parent);

  ref<variable> fresh(kind::any k=kind::term()) const;
  
  mono substitute(const mono& t) const;
  
  poly generalize(const mono& t) const;
  mono instantiate(const poly& p) const;
  
  void unify(mono from, mono to);

  // define variable, generalizing t at current depth before inserting
  state& def(symbol name, mono t);

  // TODO deftype(name, sig)
  
  // 
  friend ref<state> scope(ref<state> parent) {
    return make_ref<state>(parent);
  }
  
};


struct mono : variant<cst, var, app> {
  using mono::variant::variant;

  ::kind::any kind() const;
  
  // convenience constructor for applications
  mono operator()(mono arg) const;
};

mono infer(const ref<state>& s, const ast::expr& self);

  

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
  const extern mono func, io;
  const extern mono record, empty;

  // type as value reification
  const extern mono ty;
  
  mono ext(symbol attr);

  // convenience: build function types
  mono operator>>=(mono lhs, mono rhs);

// convenience: build record types
struct row {
  const symbol attr;
  const mono head;

  row(symbol attr, mono head) : attr(attr), head(head) { }

  mono operator|=(mono tail) const {
    return ext(attr)(head)(tail);
  }
};


}



#endif
