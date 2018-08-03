#include "tool.hpp"

namespace tool {
std::string quote(const std::string& s, char q) {
  return q + s + q;
}

std::string type_name(const std::type_info& self) {
  return self.name();
}

}
