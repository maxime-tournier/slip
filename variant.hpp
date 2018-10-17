#ifndef SLAP_VARIANT_HPP
#define SLAP_VARIANT_HPP

#include <type_traits>
#include <utility>
#include <iosfwd>

#include <cassert>

namespace {
  template<std::size_t, class ... Args>
  struct helper;

  template<std::size_t I, class T>
  struct helper<I, T> {
    static constexpr std::size_t index(const T*) { return I; }

    static void construct(void* dest, const T& source) { new (dest) T(source); }
    static void construct(void* dest, T&& source) { new (dest) T{std::move(source)}; }
  };


  template<std::size_t I, class T, class ... Args>
  struct helper<I, T, Args...> : helper<I + 1, Args... >, helper<I, T> {
    using this_type = helper<I, T>;
    using other_types = helper<I + 1, Args... >;

    using this_type::index;
    using other_types::index;

    using this_type::construct;
    using other_types::construct;
  };
  

}
  
  
template<class ... Args>
class variant {
  using storage_type = typename std::aligned_union<0, Args...>::type;
  storage_type storage;

  using index_type = unsigned char;
  index_type index;

  using helper_type = helper<0, Args...>;

  template<class T, index_type index=helper_type::index((typename std::decay<T>::type*) 0)>
  struct index_of {
    static constexpr std::size_t value = index;
  };
  
public:

  // unsafe cast
  template<class T, index_type index=index_of<T>::value>
  const T& cast() const {
    assert(index == this->index);
    return reinterpret_cast<const T&>(storage);
  }

  template<class T, index_type index=index_of<T>::value>
  T& cast() {
    assert(index == this->index);
    return reinterpret_cast<T&>(storage);
  }

  // accessors
  template<class T, index_type index=index_of<T>::value>
  const T* get() const {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  template<class T, index_type index=index_of<T>()>
  T* get() {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  // constructors/destructors
  template<class T, index_type index=index_of<T>::value>
  variant(T&& value) : index(index) {
    helper_type::construct(&storage, std::forward<T>(value));
  }

  variant(variant&& other) : index(other.index) {
    match([&](const Args& self) {
        new (&storage) Args(std::move(other.cast<Args>()));
      }...);
  }
  
  variant(const variant& other) : index(other.index) {
    match([&](const Args& self) {
        new (&storage) Args(other.cast<Args>());
      }...);
  }

  ~variant() {
    match([](const Args& self) { self.~Args(); }...);
  }

  // visitors (const)
  template<class Func, class ... Params>
  auto visit(Func&& func, Params&& ... params) const {
    using ret_type = typename std::common_type<decltype(func(this->template cast<Args>(),
                                                             std::forward<Params>(params)...))...>::type;
    using self_type = decltype(*this);
    using thunk_type = ret_type (*) (self_type&&, Func&&, Params&&...);
    
    static const thunk_type table[] = {
      [](self_type& self, Func&& func, Params&& ... params) -> ret_type {
        return std::forward<Func>(func)(self.template cast<Args>(), std::forward<Params>(params)...);
      }...
    };
     
    return table[index](*this, std::forward<Func>(func), std::forward<Params>(params)...);
  }

  // visitors (non-const)
  template<class Func, class ... Params>
  auto visit(Func&& func, Params&& ... params) {
    using ret_type = typename std::common_type<decltype(func(this->template cast<Args>(),
                                                             std::forward<Params>(params)...))...>::type;
    using self_type = decltype(*this);
    using thunk_type = ret_type (*) (self_type&&, Func&&, Params&&...);
    
    static const thunk_type table[] = {
      [](self_type& self, Func&& func, Params&& ... params) -> ret_type {
        return std::forward<Func>(func)(self.template cast<Args>(), std::forward<Params>(params)...);
      }...
    };
    
    return table[index](*this, std::forward<Func>(func), std::forward<Params>(params)...);
  }

  // pattern matching with lambdas
  template<class ... Func>
  auto match(const Func& ... func) const {
    struct overload : Func... {
      overload(const Func& ... func) : Func(func)... { }
    };
    
    return visit(overload(func...));
  }

  template<class ... Func>
  auto match(const Func& ... func) {
    struct overload : Func... {
      overload(const Func& ... func) : Func(func)... { }
    };

    return visit(overload(func...));
  }

  // copy assignment
  variant& operator=(const variant& other) {
    if(this == &other) return *this;
    
    if(index != other.index) {
      // destuct, change type, copy construct      
      this->~variant();
      index = other.index;
      match([&](const Args& self) {
          new (&storage) Args(other.cast<Args>());
        }...);
    } else {
      // copy assign
      match([&](Args& self) {
          self = other.cast<Args>();
        }...);
    }

    return *this;
  }

  // move-assignment
  variant& operator=(variant&& other) {
    if(this == &other) return *this;
    
    if(index != other.index) {
      // destuct, change type, copy construct      
      this->~variant();
      index = other.index;
      match([&](const Args& self) {
          new (&storage) Args(std::move(other.cast<Args>()));
        }...);
    } else {
      // copy assign
      match([&](Args& self) {
          self = std::move(other.cast<Args>());
        }...);
    }

    return *this;
  }
  


  // convenience
  friend std::ostream& operator<<(std::ostream& out, const variant& self) {
    self.match([&](const Args& self) {
        out << self;
      }...);
    return out;
  }


  bool operator==(const variant& other) const {
    if(other.index != index) return false;
    return match([&](const Args& self) {
        return self == other.cast<Args>();
      }...);
  }

  bool operator!=(const variant& other) const {
    return !operator==(other);
  }



};


#endif
