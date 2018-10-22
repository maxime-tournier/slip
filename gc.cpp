#include "gc.hpp"

namespace eval {

  gc* gc::all = nullptr;

  void gc::sweep() {
    for(gc** it = &all; *it; it = &(*it)->next) {
      if(!(*it)->marked) {
        gc* obj = *it;
        *it = (*it)->next;
        delete obj;
      } else {
        (*it)->marked = false;
      }
    }
  }
  
}
