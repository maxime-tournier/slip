#ifndef MAYBE_HPP
#define MAYBE_HPP

// a maybe monad
template<class T> class maybe;

namespace detail {

// ensures T is a maybe<U>
template<class T>
struct require_maybe;

template<class T>
struct require_maybe< maybe<T> > {
    using type = maybe<T>;
};

}

struct none { };

template<class T>
class maybe {
    typename std::aligned_union<0, T>::type storage;
    const bool set;
public:
    using value_type = T;
    
    // monadic return (none/just)
    maybe(none = {}) : set(false) { }
    maybe(const T& value) : set(true) { new (&storage) T(value); }
    maybe(T&& value) : set(true) { new (&storage) T(std::move(value)); }

    maybe(maybe&& other) : set(other.set) {
      if(set) new (&storage) T(std::move(other.get()));
    }

    maybe(const maybe& other) : set(other.set) {
      if(set) new (&storage) T(other.get());
    }
  
    // unclear whether we actually need these at all
    maybe& operator=(maybe&& other) = delete; 
    maybe& operator=(const maybe& other) = delete;
    
    // monadic bind
    template<class F>
    using bind_type = typename std::result_of<F (const T& )>::type;
    
    template<class F>
    typename detail::require_maybe< bind_type<F> >::type
    operator>>(const F& f) const {
        if(!set) return {};
        return f(get());
    }

    // TODO bind from temporary
    
    ~maybe() { 
        if(set) get().~T();
    }


    // monad escape
    explicit operator bool () const { return set; }
    
    const T& get() const {
        if(!set) throw std::runtime_error("accessing none");
        return *reinterpret_cast<const T*>(&storage);
    }

    T& get() {
        if(!set) throw std::runtime_error("accessing none");
        return *reinterpret_cast<T*>(&storage);
    }

};


// convenience constructors
template<class T>
static maybe<typename std::decay<T>::type> just(T&& value) {
  return {std::forward<T>(value)};
}


// functor map
template<class T, class F>
static maybe<typename std::result_of<F(T)>::type> map(const maybe<T>& x, const F& f) {
    if(x) return f(x.get());
    return {};
}



#endif
