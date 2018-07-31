#include "tool.hpp"

namespace tool {
  std::string quote(const std::string& s, char q) {
    return q + s + q;
  }
}
