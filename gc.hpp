#ifndef SLIP_GC_HPP
#define SLIP_GC_HPP

// #include <iostream>

namespace eval {

  class gc {
    gc* next;
    bool flag;
    static gc* first;
  public:

    inline gc() : next(first), flag(false) {
      first = this;
    }

    inline void mark() { flag = true; }
    inline bool marked() const { return flag; }

    static void sweep(bool debug=false);
  };
  
  
}


#endif
