#include "import.hpp"

#include <fstream>

namespace import {

  static const std::string ext = ".el";
  
  static std::ifstream resolve(symbol name) {
    // TODO iterate sys.path
    const std::string filename = name.get() + ext;
    return std::ifstream(filename);
  }

  void load(symbol name, std::function<void(ast::expr)> cont) {
    std::ifstream ifs = resolve(name);
    if(!ifs.good()) {
      throw error("package " + tool::quote(name.get()) + " not found");
    }

    // TODO cache results    
    ast::expr::iter(ifs, cont);
  }


  type::mono typecheck(symbol name) {
    using namespace type;
    mono result = type::unit;
    
    load(name, [](ast::expr e) {
      // TODO 
    });
    
    return result;
  }
  
  
}
