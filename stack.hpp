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
  using iterator_type = typename storage_type::iterator;

  std::size_t top;
  
public:
  stack(std::size_t size) : storage(size), top(0) { }

  T* allocate(std::size_t n) {
    T* res = reinterpret_cast<T*>(&storage[top]);
    top += n;
    assert(top <= storage.size() && "stack overflow");
    
    return res;
  }

  void deallocate(T* ptr, std::size_t n) {
    assert(n <= top && "stack corruption"); 
    top -= n;
    assert(ptr == reinterpret_cast<T*>(&storage[top]) && "stack corruption");
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
