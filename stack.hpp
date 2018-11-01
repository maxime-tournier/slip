#ifndef SLIP_STACK_HPP
#define SLIP_STACK_HPP

#include <vector>
#include <cassert>
#include <type_traits>

template<class T, std::size_t align=alignof(T)>
class stack {
  using value_type = typename std::aligned_union<0, T>::type;

  using storage_type = std::vector<value_type>;
  storage_type storage;

  std::size_t sp;
public:
  stack(const stack& other) = default;
  stack(stack&& other) = default;
  
  T* next() { return reinterpret_cast<T*>(&storage[sp]); }
  
  stack(std::size_t size) : storage(size), sp(0) { }

  T* allocate(std::size_t n) {
    T* res = next();
    sp += n;
    assert(sp <= storage.size() && "stack overflow");

    return res;
  }

  void deallocate(T* ptr, std::size_t n) {
    assert(n <= sp && "stack corruption"); 
    sp -= n;
    assert(ptr == next() && "stack corruption");
  }

  template<class U=T>
  struct allocator {
    static_assert(sizeof(U) >= sizeof(T), "size error");
    using storage_type = stack;
    storage_type& storage;

    using value_type = U;
    
    template<class V> struct rebind { using other = typename stack<V>::allocator; };
    
    U* allocate(std::size_t n) {
      std::size_t m = (sizeof(U) * n) / sizeof(T);
      m += (m * sizeof(T) < n * sizeof(U));
      assert(m * sizeof(T) >= n * sizeof(U));
      return reinterpret_cast<U*>(storage.allocate(m));
    }
    
    void deallocate(U* ptr, std::size_t n) {
      return storage.deallocate(ptr, n);
    }    
  };
  
};

template<class T, class U=T>
using stack_allocator = typename stack<T>::template allocator<U>;


#endif
