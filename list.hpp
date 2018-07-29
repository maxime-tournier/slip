#ifndef SLAP_LIST_HPP
#define SLAP_LIST_HPP

#include <iostream>

#include "ref.hpp"

template<class T> struct cons;
template<class T> using list = ref<cons<T>>;

template<class T>
struct cons {
  T head;
  list<T> tail;

  cons(const T& head, const list<T>& tail) : head(head), tail(tail) { }
  
  friend list<T> operator>>=(const T& head, const list<T>& tail) {
    return std::make_shared<cons>(head, tail);
  }

  template<class Init, class Func>
  friend Init foldl(const Init& init, const list<T>& self, const Func& func) {
    if(!self) return init;
    return foldl(func(init, self->head), self->tail, func);
  }

  template<class Init, class Func>
  friend Init foldr(const Init& init, const list<T>& self, const Func& func) {
    if(!self) return init;
    return func(self->head, foldr(init, self->tail, func));
  }
  
  template<class Func>
  friend list< typename std::result_of<Func(T)>::type > map(const list<T>& self,
                                                            const Func& func) {
    if(!self) return {};
    return func(self->head) >>= map(self->tail, func);
  }

};


template<class T>
static std::ostream& operator<<(std::ostream& out, const list<T>& self) {
  out << '(';
  foldl(true, self, [&](bool first, const T& it) {
                      if(!first) out << ' ';
                      out << it;
                      return false;
                    });
  return out << ')';
}



#endif
