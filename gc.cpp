#include "gc.hpp"

namespace eval {

  static gc* gc::first = nullptr;

  void gc::sweep() {
    for(gc** it = &all; *it; it = &it->next) {
      if(!*it->mark) {
        gc* obj = *it;
        *it = it->next;
        delete obj;
      }
    }
  }
  
}
