#include <map>
#include <iostream>
#include <sstream>
#include <fstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "sexpr.hpp"
#include "ast.hpp"
#include "eval.hpp"

#include "parse.hpp"
#include "unpack.hpp"

#include "type.hpp"
#include "package.hpp"

const bool debug = true;


struct history {
  const std::string filename;
  history(const std::string& filename="/tmp/slap.history")
    : filename(filename) {
    read_history(filename.c_str());
  }
    
  ~history() {
    write_history(filename.c_str());
  }
};


template<class F>
static void read_loop(const F& f) {
  const history lock;
  while(const char* line = readline("> ")) {
    if(!*line) continue;
    
    add_history(line);
    std::stringstream ss(line);
	
    f(ss);
  }
};


int main(int argc, char** argv) {

  // parser::debug::stream = &std::clog;
  package pkg = package::core();
  
  static const auto handler =
    [&](std::istream& in) {
      try {
        ast::expr::iter(in, [&](ast::expr e) {
          if(debug) std::cout << "ast: " << e << std::endl;
          pkg.exec(e, [&](type::poly p, eval::value v) {
            // TODO: cleanup variables with depth greater than current in
            // substitution
            if(auto v = e.get<ast::var>()) {
              std::cout << v->name;
            }
            
            std::cout << " : " << p << std::flush
                      << " = " << v << std::endl;
          });
        });
        return true;
      } catch(sexpr::error& e) {
        std::cerr << "parse error: " << e.what() << std::endl;
      } catch(ast::error& e) {
        std::cerr << "syntax error: " << e.what() << std::endl;
      } catch(type::error& e) {
        std::cerr << "type error: " << e.what() << std::endl;
      } catch(kind::error& e) {
        std::cerr << "kind error: " << e.what() << std::endl;
      } catch(std::runtime_error& e) {
        std::cerr << "runtime error: " << e.what() << std::endl;
      }
      return false;
    };

  
  if(argc > 1) {
    const char* filename = argv[1];
    if(auto ifs = std::ifstream(filename)) {
      return handler(ifs) ? 0 : 1;
    } else {
      std::cerr << "io error: " << "cannot read " << filename << std::endl;
      return 1;
    }
  } else {
    read_loop(handler);
  }
  
  return 0;
}
