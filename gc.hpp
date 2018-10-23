#ifndef SLIP_GC_HPP
#define SLIP_GC_HPP

// #include <iostream>

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
    static void sweep(bool debug=false);    
  };
  
  
}


#endif
