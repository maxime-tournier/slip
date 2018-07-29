#ifndef SLAP_SYMBOL_HPP
#define SLAP_SYMBOL_HPP

#include <set>
#include <string>

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

  friend std::ostream& operator<<(std::ostream& out, const symbol& self) {
    return out << self.name;
  }
  
};


#endif
