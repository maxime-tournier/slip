#ifndef SLIP_GC_HPP
#define SLIP_GC_HPP

#include "eval.hpp"

namespace eval {

  class gc {
    gc* next;
    bool mark;
    
    static gc* all;
  public:
    inline gc() : next(all), mark(false) {
      first = *this;
    }

    static void sweep();    
  };
  
  
}


#endif
