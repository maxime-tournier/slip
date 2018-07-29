#ifndef SLAP_ENVIRONMENT_HPP
#define SLAP_ENVIRONMENT_HPP

#include <map>

#include "ref.hpp"
#include "symbol.hpp"
#include "list.hpp"

// environments
template<class T>
struct environment {
  std::map<symbol, T> locals;
  ref<environment> parent;

  friend ref<environment> augment(const ref<environment>& self,
                                  const list<symbol>& args,
                                  const list<T>& values) {
    auto res = std::make_shared<environment>();
    res->parent = self;
    foldl(args, values, [&](const list<symbol>& args, const T& self) {
        if(!args) throw std::runtime_error("not enough args");
        res->locals.emplace(args->head, self);
        return args->tail;
      });

    return res;
  };
};




#endif
