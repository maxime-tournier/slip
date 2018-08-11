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
                                                            Func func) {
    if(!self) return {};
    return func(self->head) >>= map(self->tail, func);
  }

  friend std::ostream& operator<<(std::ostream& out, const list<T>& self) {
    out << '(';
    foldl(true, self, [&](bool first, const T& it) {
        if(!first) out << ' ';
        out << it;
        return false;
      });
    return out << ')';
  }

  struct iterator {
    cons* ptr;

    using reference = const T&;
    
    bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    iterator& operator++() { ptr = ptr->tail.get(); return *this; };
    reference operator*() const { return ptr->head; }
  };

  friend iterator begin(const list<T>& self) { return {self.get()}; }
  friend iterator end(const list<T>& self) { return {nullptr}; }  

};


template<class Iterator>
static list<typename Iterator::value_type> make_list(Iterator first, Iterator last) {
  if(first == last) return nullptr;
  const auto head = *first;
  return head >>= make_list(++first, last);
}
  




#endif
