#ifndef UNPACK_HPP
#define UNPACK_HPP

#include "list.hpp"

#include "../maybe.hpp"

namespace unpack {

  // list destructuring monad: functions that try to extract a value of type U
  // from a list
  template<class U>
  struct pure_type {
    const U value;

    template<class T>
    maybe<U> operator()(const list<T>& ) const {
      return value;
    }
  };
    
  template<class U>
  static pure_type<U> pure(const U& value) {
    return {value};
  };

  // monadic bind
  template<class A, class F>
  struct bind_type {
    const A a;
    const F f;

    template<class T>
    auto operator()(const list<T>& self) const -> decltype(f(a(self).get())(self)){
      using source_type = typename std::decay<decltype(a(self).get())>::type;
      using result_type = decltype(f(a(self).get())(self));

      return a(self) >> [&](const source_type& value) -> result_type {
                          if(self) return f(value)(self->tail);
                          return {};
                        };
    }
  };

  template<class A, class F>
  static bind_type<A, F> operator>>(A a, F f) {
    return {a, f};
  };
  
  // coproduct
  template<class LHS, class RHS>
  struct coproduct_type {
    const LHS lhs;
    const RHS rhs;

    template<class T>
    auto operator()(const list<T>& self) const -> decltype(lhs(self)) {
      using lhs_type = decltype(lhs(self));
      using rhs_type = decltype(rhs(self));        
      static_assert( std::is_same<lhs_type, rhs_type>::value,
                     "value types must agree");
        
      if(auto value = lhs(self)) {
        return value.get();
      } else if(auto value = rhs(self)){
        return value.get();
      } else {
        return {};
      }
    }
  };

  template<class LHS, class RHS>
  static coproduct_type<LHS, RHS> operator|(LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }

  // fail
  template<class U>
  struct fail {

    template<class T>
    maybe<U> operator()(const list<T>& ) const {
      return {};
    }
  };

  // error
  template<class U, class Error>
  struct error_type {
    const Error error;

    template<class T>
    maybe<U> operator()(const list<T>& self) const {
      throw error;
    }
  };
      

  template<class U, class Error>
  static error_type<U, Error> error(Error e) { return {e}; };

  // type erasure
  template<class T, class U>
  using monad = std::function<maybe<U>(const list<T>&)>;

  template<class F>
  struct empty_type {
    const F f;

    template<class T>
    auto operator()(const list<T>& self) const -> decltype(f(self)) {
      if(self) return {};
      return f(self);
    }
  };

  template<class F>
  static empty_type<F> empty(F f) { return {f}; }
    
  // list head
  struct pop {
    template<class T>
    maybe<T> operator()(const list<T>& self) const {
      if(!self) return {}; 
      return self->head;
    }
  };
  
  // get list head with given type, if possible
  template<class U>
  struct pop_as {
    template<class T>
    maybe<U> operator()(const list<T>& self) const {
      return pop()(self) >> [](const T& head) -> maybe<U> {
        if(auto value = head.template get<U>()) {
          return *value;
        }
        return {};
      };
    }
  };


  
  // maybe monad escape (will throw if unsuccessful)
  template<class F>
  struct run_type {
    const F f;

    template<class T>
    static T value(const T& );

    template<class T>
    auto operator()(const list<T>& self) const -> decltype(value(f(self).get())) {
      return f(self).get();
  }
  };
  
  template<class F>
  static run_type<F> run(F f) { return {f}; }
  
}

#endif
