#ifndef SLAP_SYMBOL_HPP
#define SLAP_SYMBOL_HPP

#include <string>

class symbol {
  const char* name;
public:
  
  explicit symbol(const std::string& name);

  inline const char* get() const { return name; }
  inline bool operator<(const symbol& other) const { return name < other.name; }
  inline bool operator==(const symbol& other) const { return name == other.name; }

  friend std::ostream& operator<<(std::ostream& out, const symbol& self);
  
};


#endif
