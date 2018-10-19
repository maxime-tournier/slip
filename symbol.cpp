#include "symbol.hpp"

#include <set>
#include <iostream>

symbol::symbol(const std::string& name) {
  static std::set<std::string> table;
  this->name = table.insert(name).first->c_str();
}

std::ostream& operator<<(std::ostream& out, const symbol& self) {
  return out << self.name;
}

