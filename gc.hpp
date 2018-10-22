#ifndef SLIP_GC_HPP
#define SLIP_GC_HPP

#include "eval.hpp"

namespace eval {

  class gc {
    gc* next;
    bool marked;
    
    static gc* all;
  public:
    inline gc() : next(all), marked(false) {
      all = this;
    }

    inline void mark() { marked = true; }
    static void sweep();    
  };
  
  
}


#endif
