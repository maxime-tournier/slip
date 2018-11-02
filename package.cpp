#include "package.hpp"

#include <fstream>

#include "tool.hpp"
#include "ast.hpp"

#include <vector>

namespace package {

  void iter(symbol name, std::function<void(ast::expr)> func) {
    const std::string filename = resolve(name);

    std::ifstream in(filename);
    ast::expr::iter(in, [&](const ast::expr& e) {
      func(e);
    });
  }

  static const std::string ext = ".el";

  static bool exists(std::string path) {
    return std::ifstream(path).good();
  }

// TODO better/portable/what not
  static std::string join(const std::string& path, const std::string& file) {
    return path + "/" + file;
  }

  std::string resolve(symbol name) {
    for(const std::string& prefix : path()) {
      const std::string filename = join(prefix, name.get() + ext);
      if(exists(filename)) {
        return filename;
      }
    }
  
    throw std::runtime_error("package " + tool::quote(name.get()) + " not found");
  }

  std::vector<std::string>& path() {
    static std::vector<std::string> instance = {"", SLIP_PATH};
    return instance;
  }

}
