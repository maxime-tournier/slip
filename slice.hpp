#ifndef SLAP_SLICE_HPP
#define SLAP_SLICE_HPP

#include "maybe.hpp"
#include "list.hpp"

namespace slice {
  // list processing monad: computations on lists that try to extract a value
  // from a contiguous slice, returning the remainder of the list and the value
  template<class T, class A>
  struct slice {
    const maybe<A> result;
    const list<T> rest;
  };

  template<class T, class A>
  using monad = std::function<slice<T, A>(const list<T>& )>;
  
  // monadic injection
  template<class A>
  struct pure_type {
    const A value;
    
    template<class T>
    slice<T, A> operator()(const list<T>& self) const {
      return {value, self};
    }
    
  };

  template<class A>
  static pure_type<A> pure(A a) { return {a};}

  
  // monadic bind
  template<class MA, class Func>
  struct bind_type {
    const MA ma;
    const Func func;

    // type helpers
    template<class T>
    auto source_slice_type(const list<T>& self) const -> decltype(ma(self));

    template<class T, class A>
    A result_type(const slice<T, A>&) const;
    
    template<class A>
    auto target_slice_type(const A& self) const -> decltype(func(self));
    
    template<class T>
    auto operator()(const list<T>& self) const {
      using type = decltype(target_slice_type(result_type(source_slice_type(self))));
      
      const auto ra = ma(self);
      if(!ra.result) {
        return type{{}, self};
      }
      
      return func(ra.result.get())(ra.rest);
    }
    
  };

  // abort computation
  template<class A>
  struct fail_type {
    template<class T>
    slice<T, A> operator()(const list<T>& self) const {
      return {{}, self};
    }
  };

  
  // coproduct
  template<class LHS, class RHS>
  struct coproduct_type {
    const LHS lhs;
    const RHS rhs;

    template<class T>
    auto operator()(const list<T>& self) const {
      const auto left = lhs(self);
      if(left.result) return left;
      return rhs(self);
    }
    
  };

  template<class LHS, class RHS>
  static coproduct_type<LHS, RHS> operator|(LHS lhs, RHS rhs) { return {lhs, rhs}; }
  
  
  // conditional pop
  template<class Pred>
  struct pop_if_type {
    const Pred pred;
    
    template<class T>
    slice<T, T> operator()(const list<T>& self) const {
      if(!self || !pred(self->head)) return {{}, self};
      return {self->head, self->tail};
    }
  };

  template<class Pred>
  static pop_if_type<Pred> pop_if(Pred pred) { return {pred}; }

  // unconditional pop
  static const auto pop = [] { return pop_if([](auto) { return true; }); };


  // final monadic injection: any computation after this one will fail
  template<class A>
  struct done_type {
    const A value;

    template<class T>
    slice<T, A> operator()(const list<T>& self) const {
      if(self) return {{}, self};
      return {value, self};
    }
  };

  template<class A>
  static done_type<A> done(A a) { return {a}; }

  // peek at current list (useful for debugging)
  struct peek {
    template<class T>
    slice<T, list<T>> operator()(const list<T>& self) const {
      return {self, self};
    }
  };
  
}



#endif
