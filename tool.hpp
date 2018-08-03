#ifndef SLAP_TOOL_HPP
#define SLAP_TOOL_HPP

#include <string>

namespace tool {

std::string quote(const std::string& s, char q='"');
std::string type_name(const std::type_info& self);

}

#endif
