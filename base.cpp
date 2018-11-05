#include "base.hpp"
#include <iostream>

std::ostream& operator<<(std::ostream& out, unit self) {
  return out << "()";
}
 
