#include "gc.hpp"

#include <cassert>
#include <iostream>

namespace eval {

  gc* gc::all = nullptr;

  void gc::sweep(bool debug) {
    gc** it = &all;
    while(*it) {
      if(debug) {
        std::clog << "processing: " << *it << std::endl;
      }
      if(!(*it)->marked) {
        gc* obj = *it; assert(obj);
        *it = (*it)->next;
        if(debug) {
          std::clog << "deleting: " << obj << std::endl;
        }
        delete obj;
      } else {
        (*it)->marked = false;
        it = &(*it)->next;
      }
    }

    return;
  }
  
}
