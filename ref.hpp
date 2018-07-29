#ifndef SLAP_REF_HPP
#define SLAP_REF_HPP

#include <memory>

// refcounting
template<class T>
using ref = std::shared_ptr<T>;

template<class T, class ... Args>
static ref<T> make_ref(Args&& ... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}


#endif
