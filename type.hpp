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

// substitutions
class substitution {
  using key_type = ref<variable>;
  using map_type = std::map<key_type, mono>;
  map_type map;
public:
  
  // substitute to all type variables in t
  mono operator()(mono t) const;

  // add from -> to
  void link(key_type from, mono to);
};


// type state
struct state : environment<poly> {
  const std::size_t level;
  const ref<substitution> subst;

  state();
  state(const ref<state>& parent);

  ref<variable> fresh(kind::any k=kind::term) const;
};



// monotypes
struct mono : variant<ref<constant>,
                      ref<variable>,
                      ref<application>> {
  
  using mono::variant::variant;

  ::kind::any kind() const;
  
  static mono infer(const ref<state>& s, const ast::expr& self);
  
  friend std::ostream& operator<<(std::ostream& out, const mono& self);

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

  // note: must be properly substituted first
  static poly generalize(const mono& self, std::size_t depth=0);
};


// constants
const extern ref<constant> unit, boolean, integer, real;

const extern ref<constant> func, list;

// convenience: build function types
mono operator>>=(mono lhs, mono rhs);




}



#endif
