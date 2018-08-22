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
  
  struct destruct {
    using type = void;
    template<class T>
    void operator()(const T& self) const { self.~T(); }
  };

  struct move_construct {
    using type = void;
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(std::move(source));
    }
  };

  struct copy_construct {
    using type = void;
    
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(source);
    }
  };


  struct equals {
    using type = bool;
    
    template<class T, class Variant>
    bool operator()(const T& self, const Variant& other) const {
      return self == other.template cast<T>();
    }
  };


template<class Ret, class ... Func>
struct overload : Func... {
  using type = Ret;
  overload(const Func& ... func) : Func(func)... { }
};


  // helpers
  struct write {
    using type = void;
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }
  };
  
}
  
  
template<class ... Args>
class variant {
  using storage_type = typename std::aligned_union<0, Args...>::type;
  storage_type storage;

  using index_type = unsigned char;
  index_type index;

  using helper_type = helper<0, Args...>;

  template<class T, class Ret, class Self, class Func, class ... Params>
  static Ret thunk(Self&& self, Func&& func, Params&& ... params) {
    return std::forward<Func>(func)(std::forward<Self>(self).template cast<T>(),
                                    std::forward<Params>(params)...);
  }

public:

  // unsafe casts
  template<class T,
           index_type index = helper_type::index((typename std::decay<T>::type*) 0)>
  const T& cast() const {
    assert(index == this->index);
    return reinterpret_cast<const T&>(storage);
  }

  template<class T,
           index_type index = helper_type::index((typename std::decay<T>::type*) 0)>
  T& cast() {
    assert(index == this->index);
    return reinterpret_cast<T&>(storage);
  }

  
  // accessors
  template<class T,
           index_type index = helper_type::index((typename std::decay<T>::type*) 0)>
  const T* get() const {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  template<class T,
           index_type index = helper_type::index((typename std::decay<T>::type*)0)>
  T* get() {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  // constructors/destructors
  template<class T,
           index_type index = helper_type::index((typename std::decay<T>::type*) 0)>
  variant(T&& value) : index(index) {
    helper_type::construct(&storage, std::forward<T>(value));
  }

  // TODO check whether this one actually works (visit is const)
  variant(variant&& other) : index(other.index) {
    other.visit(move_construct(), &storage);
  }
  
  variant(const variant& other) : index(other.index) {
    other.visit(copy_construct(), &storage);
  }

  ~variant() { visit(destruct()); }

  // visitors
  template<class Func, class ... Params>
  typename std::decay<Func>::type::type visit(Func&& func, Params&& ... params) const {
    using ret_type = typename std::decay<Func>::type::type;
    using self_type = const variant&;
    using thunk_type = ret_type (*) (self_type&&, Func&&, Params&&...);
    
    static const thunk_type table[] =
      {thunk<Args, ret_type, self_type, Func, Params...>...};
    
    return table[index](*this, std::forward<Func>(func),
                        std::forward<Params>(params)...);
  }

  template<class Ret = void, class ... Func>
  Ret match(const Func& ... func) const {
    return visit(overload<Ret, Func...>(func...));
  }

  friend std::ostream& operator<<(std::ostream& out, const variant& self) {
    self.visit(write(), out);
    return out;
  }


  bool operator==(const variant& other) const {
    if(other.index != index) return false;
    return visit(equals(), other);
  }

  bool operator!=(const variant& other) const {
    return !operator==(other);
  }


  // assignment
  variant& operator=(const variant& other) {
    if(this != &other) {
      // TODO optimize when types match
      this->~variant();
      index = other.index;
      other.visit(copy_construct(), &storage);
    }
    return *this;
  }

};


#endif
