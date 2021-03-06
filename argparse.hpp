#ifndef SLAP_ARGPARSE_HPP
#define SLAP_ARGPARSE_HPP

#include "slice.hpp"

#include <sstream>
#include <iomanip>

namespace argparse {

  using namespace slice;

  // using the slice monad on list<const char*> with item values
  using thrower = std::function<void()>;
  using item = std::pair<const char*, thrower>;

  template<class T>
  static const auto argument(const char* name) {
    return pop() >> [=](const char* value) {
      T result;
      using std::operator>>;
      if(std::stringstream(value) >> result) {
        // el cheapo type erasure: close over result and throw its captured
        // address to recover it (must be cought with the right type)
        const item res = {name, [result] { throw &result; }};
        return pure(res);
      }
      
      throw std::runtime_error("type error when parsing argument "
                               + tool::quote(name));
    };
  };
  
  template<class T>
  static const auto option(const char* name) {
    const std::string target = "--" + std::string(name);
    return pop_if([=](const char* item) {
        return item == target;
      }) >> argument<T>;
  };


  static const auto flag(const char* name) {
    const auto parser = [name](std::string target, bool value) {
      return pop_if([=](const char* item) {
          return item == target;
        }) >> [name, value](const char*) {
        const item res = {name, [value] { throw &value; }};
        return pure(res);
      };
    };
    
    return parser("--" + std::string(name), true)
      | parser("--no-" + std::string(name), false);
  };

  
  ////////////////////////////////////////////////////////////////////////////////
  class result {
    std::map<std::string, thrower> parsed;
  public:
    inline void set(const char* name, thrower value) {
      if(!parsed.emplace(name, value).second) {
        throw std::runtime_error("duplicate option " + tool::quote(name));
      }
    }
    
    template<class T>
    const T* get(const char* name) const {
      auto it = parsed.find(name);
      if(it == parsed.end()) return nullptr;

      try {
        it->second();
      } catch(const T* result) {
        return result;
      } catch(...) {
        throw std::runtime_error("type error when accessing argument "
                                 + tool::quote(name));
      }

      throw std::logic_error("thrower did not throw");
    }

    inline bool flag(const char* name, bool default_value) const {
      if(auto ptr = get<bool>(name)) {
        return *ptr;
      } else {
        return default_value;
      }
    }
  };
  
  class parser {
    monad<const char*, item> options = fail<item>();
    std::vector<std::pair<std::string, maybe<std::string>>> help;

    void add(std::string name, const char* doc) {
      help.emplace_back(name, doc ? just(std::string(doc)) : maybe<std::string>());
    }
    
  public:
    result parse(int argc, const char** argv) const {
      const list<const char*> args = make_list(argv, argv + argc);

      auto first = pop()(args);
      assert(first);
      
      const std::string name = first.get();
      
      result res;
      slice<const char*, item> current = {{}, first.rest};
      while((current = options(current.rest))) {
        res.set(current.get().first, current.get().second);
      }

      if(current.rest) {
        std::stringstream ss;
        ss << "unprocessed options: " << current.rest;
        throw std::runtime_error(ss.str());
      }
      return res;
    }

    parser& flag(const char* name, const char* doc=nullptr) {
      add("--" + std::string(name), doc);
      options = argparse::flag(name) | options;
      return *this;
    }

    template<class T>
    parser& option(const char*  name, const char* doc=nullptr) {
      add("--" + std::string(name), doc);
      options = argparse::option<T>(name) | options;
      return *this;
    }

    template<class T>
    parser& argument(const char* name, const char* doc=nullptr) {
      add(name, doc);      
      options = options | argparse::argument<T>(name);
      return *this;
    }

    void describe(std::ostream& out) const {
      out << "available options:" << std::endl;
      std::size_t width = 24;
      for(auto it : help) {
        out << "\t" << std::setw(width) << std::left << it.first;
        if(it.second) out << std::setw(width) << std::left << it.second.get();
        out << std::endl;
      }
    }
    
  };
  

  // TODO higher-level class that provides help message etc
}


#endif
