// -*- compile-command: "c++ -o slip2 slip.cpp" -*-

// variant
#include <type_traits>
#include <utility>
#include <new>

// symbol
#include <set>

// refs
#include <memory>

//
#include <map>

#include <iostream>

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
    template<class T>
    void operator()(const T& self) const { self.~T(); }
  };

  struct move {
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(std::move(source));
    }
  };

  struct copy {
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(source);
    }
  };


  template<class ... Func>
  struct overload : Func... {
    overload(const Func& ... func) : Func(func)... { }
  };


  // helpers
  struct write {
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

  // unsafe casts
  template<class T>
  const T& cast() const { return reinterpret_cast<const T&>(storage); }

  template<class T>
  T& cast() { return reinterpret_cast<T&>(storage); }

public:

  // accessors
  template<class T, index_type index = helper_type::index( (typename std::decay<T>::type*) 0 )>
  const T* get() const {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  template<class T, index_type index = helper_type::index( (typename std::decay<T>::type*) 0 )>
  T* get() {
    if(this->index == index) { return &cast<T>(); }
    return nullptr;
  }

  // constructors/destructors
  template<class T, index_type index = helper_type::index( (typename std::decay<T>::type*) 0 )>
  variant(T&& value) : index(index) {
    helper_type::construct(&storage, std::forward<T>(value));
  }

  variant(variant&& other) : index(other.index) {
    other.visit<void>(move(), &storage);
  }
  
  variant(const variant& other) : index(other.index) {
    other.visit<void>(copy(), &storage);
  }

  ~variant() { visit<void>(destruct()); }

  // visitors
  template<class Ret = void, class Func, class ... Params>
  Ret visit(Func&& func, Params&& ... params) const {
    using ret_type = Ret;
    using self_type = const variant&;
    using thunk_type = ret_type (*) (self_type&&, Func&&, Params&&...);
    
    static const thunk_type table[] = {thunk<Args, ret_type, self_type, Func, Params...>...};
    
    return table[index](*this, std::forward<Func>(func), std::forward<Params>(params)...);
  }

  template<class Ret = void, class ... Func>
  Ret match(const Func& ... func) const {
    return visit<Ret>(overload<Func...>(func...));
  }

  friend std::ostream& operator<<(std::ostream& out, const variant& self) {
    self.visit(write(), out);
    return out;
  }
  
};


// base types
using boolean = bool;
using integer = long;
using real = double;

class symbol {
  const char* name;
public:
  
  explicit symbol(const std::string& name) {
    static std::set<std::string> table;
    this->name = table.insert(name).first->c_str();
  }

  const char* get() const { return name; }
  bool operator<(const symbol& other) const { return name < other.name; }
  bool operator==(const symbol& other) const { return name == other.name; }

  friend std::ostream& operator<<(std::ostream& out, const symbol& self) { return out << self.name; }
};

// lists
template<class T>
using ref = std::shared_ptr<T>;

template<class T> struct cons;
template<class T> using list = ref<cons<T>>;

template<class T>
struct cons {
  T head;
  list<T> tail;

  cons(const T& head, const list<T>& tail) : head(head), tail(tail) { }
  
  friend list<T> operator>>=(const T& head, const list<T>& tail) {
    return std::make_shared<cons>(head, tail);
  }

  template<class Init, class Func>
  friend Init foldl(const Init& init, const list<T>& self, const Func& func) {
    if(!self) return init;
    return foldl(func(init, self->head), self->tail, func);
  }

  template<class Init, class Func>
  friend Init foldr(const Init& init, const list<T>& self, const Func& func) {
    if(!self) return init;
    return func(self->head, foldr(init, self->tail, func));
  }
  
  template<class Func>
  friend list< typename std::result_of<Func(T)>::type > map(const list<T>& self, const Func& func) {
    if(!self) return {};
    return func(self->head) >>= map(self->tail, func);
  }

  friend std::ostream& operator<<(std::ostream& out, const list<T>& self) {
    out << '(';
    foldl(true, self, [&](bool first, const T& it) {
        if(!first) out << ' ';
        out << it;
        return false;
      });
    return out << ')';
  }
};


// s-expressions
struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
  using list = list<sexpr>;
};

struct env;

// values
struct lambda {
  const list<symbol> args;
  const sexpr body;
  const ref<env> scope;

  friend std::ostream& operator<<(std::ostream& out, const lambda& self) { return out << "#<lambda>"; }
};

struct value : variant<real, integer, boolean, symbol, lambda, list<value> > {
  using value::variant::variant;
  using list = list<value>;
};


// environments
struct env {
  std::map<symbol, value> locals;
  ref<env> parent;

  friend ref<env> augment(const ref<env>& self, const list<symbol>& args, const value::list& values) {
    auto res = std::make_shared<env>();
    res->parent = self;
    foldl(args, values, [&](const list<symbol>& args, const value& self) {
        if(!args) throw std::runtime_error("not enough args");
        res->locals.emplace(args->head, self);
        return args->tail;
      });

    return res;
  };
};



using eval_type = value (*)(const ref<env>&, const sexpr& );
static std::map<symbol, eval_type> special;

static std::runtime_error unimplemented() { return std::runtime_error("unimplemented"); }

static value eval(const ref<env>& e, const sexpr& expr);

static value apply(const ref<env>& e, const sexpr& func, const sexpr::list& args) {
  return eval(e, func).match<value>
    ([](value) -> value { throw std::runtime_error("invalid type in application"); },
     [&](const lambda& self) {
       return eval(augment(e, self.args, map(args, [&](const sexpr& it) { return eval(e, it); })),
                   self.body);
     });
}

static value eval(const ref<env>& e, const sexpr& expr) {
  return expr.match<value>
    ([](value self) { return self; },
     [&](symbol self) {
       auto it = e->locals.find(self);
       if(it == e->locals.end()) {
         throw std::runtime_error("unbound variable: " + std::string(self.get()));
       }

       return it->second;
     },
     [&](const sexpr::list& x) {
       if(!x) throw std::runtime_error("empty list in application");

       // special forms
       if(auto s = x->head.get<symbol>()) {
         auto it = special.find(*s);
         if(it != special.end()) {
           return it->second(e, x->tail);
         }
       }

       // regular apply
       return apply(e, x->head, x->tail);       
     });
}



int main(int, char**) {

  const sexpr e = symbol("michel");
  
  auto global = std::make_shared<env>();
  global->locals.emplace("michel", 14l);
  
  std::clog << e << std::endl;
  
  std::clog << eval(global, e) << std::endl;
  return 0;
}
