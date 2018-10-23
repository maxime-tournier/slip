#include "gc.hpp"

#include <cassert>
#include <iostream>

namespace eval {

  gc* gc::first = nullptr;

  void gc::sweep(bool debug) {
    gc** it = &first;
    while(*it) {
      if(debug) {
        std::clog << "processing:\t" << *it << std::endl;
      }
      if(!(*it)->flag) {
        gc* obj = *it; assert(obj);
        *it = (*it)->next;
        if(debug) {
          std::clog << "deleting:\t" << obj << std::endl;
        }
        delete obj;
      } else {
        (*it)->flag = false;
        it = &(*it)->next;
      }
    }

    return;
  }
  
}
