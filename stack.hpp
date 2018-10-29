#ifndef SLIP_STACK_HPP
#define SLIP_STACK_HPP

#include <vector>
#include <cassert>
#include <type_traits>

template<class T>
class stack {
  using value_type = typename std::aligned_union<0, T>::type;

  using storage_type = std::vector<value_type>;
  storage_type storage;

  std::size_t sp;
public:
  T* top() { return reinterpret_cast<T*>(&storage[sp]); }
  
  stack(std::size_t size) : storage(size), sp(0) { }

  T* allocate(std::size_t n) {
    T* res = top();
    sp += n;
    assert(sp <= storage.size() && "stack overflow");
    
    return res;
  }

  void deallocate(T* ptr, std::size_t n) {
    assert(n <= sp && "stack corruption"); 
    sp -= n;
    assert(ptr == top() && "stack corruption");
  }
  
  struct allocator {
    stack& storage;

    using value_type = T;
    template<class U> struct rebind { using other = typename stack<U>::allocator; };
    
    T* allocate(std::size_t n) { return storage.allocate(n); }
    void deallocate(T* ptr, std::size_t n) { return storage.deallocate(ptr, n); }    
  };
  
};


#endif
