#ifndef SLAP_INFER_HPP
#define SLAP_INFER_HPP

#include "type.hpp"

namespace ast {
  struct expr;
};

namespace type {

  // type substitution
  class substitution;
  
  // type state
  struct state {
    using vars_type = environment<poly>;
    using sigs_type = std::map<cst, poly>;
  
    const std::size_t level;
  
    const ref<vars_type> vars;
    const ref<sigs_type> sigs;
    
    const ref<substitution> sub;

    state();
    state(const ref<state>& parent);

    ref<variable> fresh(kind::any k=kind::term()) const;
  
    poly generalize(const mono& t) const;
    mono instantiate(const poly& p) const;
  
    void unify(mono from, mono to, logger* log=nullptr);
    
    // define variable, generalizing t at current depth before inserting
    state& def(symbol name, mono t);

    // TODO deftype(name, sig)
  
    // 
    friend ref<state> scope(ref<state> parent) {
      return make_ref<state>(parent);
    }
    
    bool debug = false;
  };


  
  mono infer(const ref<state>& s, const ast::expr& self);
  
}


#endif
