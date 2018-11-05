#ifndef SLIP_NAN_HPP
#define SLIP_NAN_HPP

#include <bitset>

namespace nan {
  // payload is large enough to hold a x86-64 pointer
  union ieee754 {
    using payload_type = unsigned long;
    using tag_type = unsigned short;
    using sign_type = bool;
    using exponent_type = unsigned int;

    double value;
    struct {
      sign_type sign: 1;
      exponent_type exponent: 11;
      bool quiet: 1;
      tag_type tag: 3;
      payload_type payload: 48;
    } bits;

    static constexpr exponent_type nan = (1u << 11) - 1;
  
    template<class Func>
    auto visit(Func func) const {
      if(bits.exponent == nan) {
        return func(bits.sign, bits.tag, bits.payload);
      } else {
        return func(value);
      }
    }


    template<class ... Funcs>
    auto match(Funcs... funcs) const {
      struct overload : Funcs... {
        overload(Funcs...funcs): Funcs(funcs)... { }
      };

      return visit(overload{funcs...});
    }

    ieee754(double value): value(value) { }
    ieee754(sign_type sign, tag_type tag, payload_type payload) {
      bits.sign = sign;
      bits.exponent = nan;
      bits.quiet = true;
      bits.tag = tag;
      bits.payload = payload;
    }

  };

  static_assert(sizeof(ieee754) == sizeof(double), "size error");


  namespace detail {
    template<std::size_t I, class ...>
    struct helper;

    template<std::size_t I>
    struct helper<I> {
      static void index() { }
    };

    template<std::size_t I, class T, class ... Args>
    struct helper<I, T, Args...> : helper<I + 1, Args...> {
      
      using helper<I + 1, Args...>::index;
      static constexpr std::size_t index(const T*) { return I; }
  
    };
  }


  template<bool...> struct bool_pack;

  template<bool... bs> 
  using all_true = std::is_same<bool_pack<bs..., true>, bool_pack<true, bs...>>;

  template<class ... T>
  class variant {
    static_assert(all_true<(sizeof(T) <= sizeof(ieee754))...>::value, "size error");
  
    static_assert(all_true<std::is_trivially_destructible<T>::value...>::value,
                  "types must be trivially destructible");
  
    static_assert(all_true<std::is_trivially_copyable<T>::value...>::value,
                  "types must be trivially copyable");
  
    static_assert(sizeof...(T) < (1 << 4), "too many types in variant");
  
    ieee754 storage;

    using index_type = unsigned short;

    static ieee754::sign_type sign(index_type index) {
      return index >> 3;
    }

    static ieee754::tag_type tag(index_type index) {
      return index & 7u;
    }

    using helper_type = detail::helper<0, T...>;
  
    template<class U, index_type index=helper_type::index((const U*)0)>
    static ieee754 make_storage(const U& value) {
      ieee754::payload_type payload = 0;
      reinterpret_cast<U&>(payload) = value;

      ieee754 res(sign(index), tag(index), payload);
      return res;
    }
  
  public:

    template<class Func, class ... Args>
    auto visit(Func func, Args&&... args) const {
      return storage.match([&](double self) {
          return func(self, std::forward<Args>(args)...);
        },
        [&](ieee754::sign_type sign,
            ieee754::tag_type tag,
            ieee754::payload_type payload) {
          using ret_type = typename std::common_type<decltype(func(std::declval<T>(),
                                                                   std::forward<Args>(args)...))...>::type;
          using thunk_type = ret_type (*)(ieee754::payload_type, const Func&, Args&&...);

          static const thunk_type table[] = {
            [](ieee754::payload_type payload, const Func& func, Args&&...args) {
              return func(reinterpret_cast<const T&>(payload), std::forward<Args>(args)...);
            }...};

          const index_type index = tag | (sign << 4);
          return table[index](payload, func, std::forward<Args>(args)...);
        });
    }

    variant(double value): storage(value) { }

    template<class U, index_type=helper_type::index((const U*)0)>
    variant(const U& value): storage(variant::make_storage(value)) {
      // std::clog << "storing: " << std::bitset<64>(storage.bits.payload) << std::endl;
      // std::clog << "hex: " << std::hex << storage.bits.payload << std::endl;      
    }

    variant(const variant& other): storage(other.storage) { }    

    template<class U, index_type index=helper_type::index((const U*)0)>
    U cast() const {
      if(std::is_same<U, double>::value) {
        return reinterpret_cast<const U&>(storage.value);
      } else {
        ieee754::payload_type payload = storage.bits.payload;
        return reinterpret_cast<const U&>(payload);
      }
    }

    template<class ... Cases>
    auto match(Cases...cases) const {
      struct overload: Cases... {
        overload(Cases...cases): Cases(cases)...{ }
      };

      return visit(overload(cases...));
    }
  
  };

}

#endif
