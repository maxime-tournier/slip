#ifndef SLIP_GC_HPP
#define SLIP_GC_HPP

// #include <iostream>
#include <utility>

template<class Tag>
class gc {

  struct block {
    virtual ~block() { }

    union {
      block* ptr;
      std::size_t bits;
    } next;
      
    block() {
      next.ptr = first;
      first = this;
    }
      
    // gc mark in lower order bits
    void set_mark(bool value) {
      if(value) next.bits |= 1ul;
      else next.bits &= ~1ul;
    }
      
    bool get_mark() const {
      return next.bits & 1ul;
    }

  };
    
  template<class T>
  struct managed : block {
    T value;

    template<class ... Args>
    managed(Args&& ... args): value(std::forward<Args>(args)...) { }
  };

  static block* first;
    
public:
    
  template<class T>
  class ref {
    using ptr_type = managed<T>*;
    ptr_type ptr;
    friend class gc;
    ref(ptr_type ptr): ptr(ptr) { }
  public:
    ref(): ptr(nullptr) { };

    explicit operator bool() const { return ptr; }
      
    T* get() const { return &ptr->value; }
    T& operator*() const { return *ptr; }
    T* operator->() const { return get();}

    inline void mark() { ptr->set_mark(true); }
    inline bool marked() const { return ptr->get_mark(); }      
  };


  template<class T, class ... Args>
  static ref<T> make_ref(Args&& ... args) {
    return {new managed<T>(std::forward<Args>(args)...)};
  }

  static void sweep() {
    block** it = &first;
    while(*it) {
      if(!(*it)->get_mark()) {
        block* obj = *it;
        *it = (*it)->next.ptr;
        delete obj;
      } else {
        (*it)->set_mark(false);
        it = &(*it)->next.ptr;
      }
    }
  }
};

template<class Tag>
typename gc<Tag>::block* gc<Tag>::first = nullptr;


#endif
