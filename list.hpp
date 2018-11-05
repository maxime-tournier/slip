#ifndef SLAP_LIST_HPP
#define SLAP_LIST_HPP

#include <iostream>

#include "ref.hpp"


template<class T> struct cons;
template<class T> using list = ref<cons<T>>;

template<class T>
struct cons {
  T head;
  
  using list = ref<cons>;
  list tail;

  cons(const T& head, const list& tail) : head(head), tail(tail) { }
  
  friend list operator>>=(const T& head, const list& tail) {
    return make_ref<cons>(head, tail);
  }

  template<class Init, class Func>
  friend Init foldl(const Init& init, const list& self, const Func& func) {
    if(!self) return init;
    return foldl(func(init, self->head), self->tail, func);
  }

  template<class Init, class Func>
  friend Init foldr(const Init& init, const list& self, const Func& func) {
    if(!self) return init;
    return func(self->head, foldr(init, self->tail, func));
  }

  friend list reverse(const list& self, const list& acc = nullptr) {
    if(!self) return acc;
    return reverse(self->tail, self->head >>= acc);
  }

  friend list concat(const list& lhs, const list& rhs) {
    if(!lhs) return rhs;
    return lhs->head >>= concat(lhs->tail, rhs);
  }

  
  template<class Func>
  friend ::list<typename std::result_of<Func(T)>::type> map(const list& self,
                                                            Func func) {
    if(!self) return {};
    return func(self->head) >>= map(self->tail, func);
  }

  friend std::ostream& operator<<(std::ostream& out, const list& self) {
    out << '(';
    foldl(true, self, [&](bool first, const T& it) {
        if(!first) out << ' ';
        out << it;
        return false;
      });
    return out << ')';
  }

  struct iterator {
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using reference = const T&;
    using pointer = const T*;
    
    cons* ptr;
    
    bool operator!=(const iterator& other) const { return ptr != other.ptr; }
    iterator& operator++() { ptr = ptr->tail.get(); return *this; };
    reference operator*() const { return ptr->head; }
  };

  friend iterator begin(const list& self) { return {self.get()}; }
  friend iterator end(const list& self) { return {nullptr}; }  

  friend std::size_t size(const list& self) {
    return foldr(0, self, [](const T&, std::size_t s) { return s + 1; });
  }
  
};


template<class Iterator>
static list<typename std::iterator_traits<Iterator>::value_type>
make_list(Iterator first, Iterator last) {
  if(first == last) return nullptr;
  const auto head = *first;
  return head >>= make_list(++first, last);
}
  




#endif
