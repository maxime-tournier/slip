#ifndef SLAP_TOOL_HPP
#define SLAP_TOOL_HPP

#include <string>
#include <utility>

namespace tool {

std::string quote(const std::string& s, char q='"');
std::string type_name(const std::type_info& self);

template<class F, class ... Args, std::size_t ... I>
static typename std::result_of< F(const Args&...) >::type
apply(const F& f, const std::tuple<const Args&...>& args, std::index_sequence<I...>) {
  return f(std::get<I>(args)...);
}

}

#endif
