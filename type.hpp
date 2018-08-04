#ifndef SLAP_TYPE_HPP
#define SLAP_TYPE_HPP

#include <set>

#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "environment.hpp"

namespace ast {
struct expr;
};

namespace kind {

struct constant;
struct constructor;

struct any : variant<ref<constant>, ref<constructor>> {
  using any::variant::variant;
};

extern const ref<constant> term;
extern const ref<constant> row;

struct constant {
  const symbol name;
};

struct constructor {
  const any from;
  const any to;
};

any operator>>=(any from, any to);

struct error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

}

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

  ref<variable> fresh(kind::any k=kind::term) const;
  
  mono substitute(const mono& t) const; 
  poly generalize(const mono& t) const;
  
  void unify(const mono& from, const mono& to);
};



// monotypes
struct mono : variant<ref<constant>,
                      ref<variable>,
                      ref<application>> {
  
  using mono::variant::variant;

  ::kind::any kind() const;
  
  static mono infer(const ref<state>& s, const ast::expr& self);
  
  friend ref<application> apply(mono ctor, mono arg);
};


struct constant {
  const symbol name;
  const ::kind::any kind;
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
const extern ref<constant> unit, boolean, integer, real;

const extern ref<constant> func, list;

// convenience: build function types
mono operator>>=(mono lhs, mono rhs);




}



#endif
