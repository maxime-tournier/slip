#ifndef SLAP_TOOL_HPP
#define SLAP_TOOL_HPP

#include <string>

#include <utility>
#include <sstream>

#include <chrono>

namespace tool {

  std::string quote(const std::string& s, char q='"');
  std::string type_name(const std::type_info& self);

  template<class Func, class ... Args, std::size_t ... I>
  static auto apply(const Func& func, const std::tuple<Args...>& args,
                    std::index_sequence<I...>) {
    return func(std::get<I>(args)...);
  }
  

  // normalized/user friendly output of a type
  template<class T>
  static std::string show(const T& self, std::size_t max=64) {
    std::stringstream ss;
    ss << self;
    
    std::string str = ss.str();
    if(str.size() > max) {
      str = str.substr(0, max) + "...";
    }
    return quote(str);
  }


  template<class Func, class Resolution = std::chrono::milliseconds,
           class Clock = std::chrono::high_resolution_clock>
  static auto timer(double* duration, Func func) {
    typename Clock::time_point start = Clock::now();
    auto res = func();
    typename Clock::time_point stop = Clock::now();
    
    std::chrono::duration<double> diff = stop - start;
    *duration = diff.count();
    return res;
  }
  
}

#endif
