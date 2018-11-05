#ifndef SLIP_BASE_HPP
#define SLIP_BASE_HPP

#include <iosfwd>

// base types
struct unit {
  friend std::ostream& operator<<(std::ostream& out, unit);
};

using boolean = bool;
using integer = long;
using real = double;

struct string;
template<class T> struct vector;


#endif
