#ifndef SLIP_VECTOR_HPP
#define SLIP_VECTOR_HPP

#include <vector>

template<class T>
struct vector : std::vector<T> {
  using std::vector<T>::vector;

  template<class Init, class Func>
  friend Init foldl(Init init, const vector& self, Func func) {
    for(auto it = self.begin(), end = self.end(); it != end; ++it) {
      init = func(init, *it);
    }
    return init;
  }

  template<class Init, class Func>
  friend Init foldr(Init init, const vector& self, Func func) {
    for(auto it = self.rbegin(), end = self.rend(); it != end; ++it) {
      init = func(init, *it);
    }
    return init;
  }
  
};



#endif
