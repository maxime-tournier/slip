#include "gc.hpp"

#include <cassert>

namespace eval {

  gc* gc::all = nullptr;

  void gc::sweep() {
    gc** it = &all;
    while(*it) {
      if(!(*it)->marked) {
        gc* obj = *it; assert(obj);
        *it = (*it)->next;
        delete obj;
      } else {
        (*it)->marked = false;
        it = &(*it)->next;
      }
    }

    return;
  }
  
}
