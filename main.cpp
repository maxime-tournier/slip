#include <map>
#include <iostream>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "sexpr.hpp"
#include "ast.hpp"
#include "eval.hpp"

#include "unpack.hpp"

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
  // TODO read history
  const history lock;
  while(const char* line = readline("> ")) {
    if(!*line) continue;
    
    add_history(line);
	std::stringstream ss(line);
	
    f(ss);
  }
  
};


static ref<env> prelude() {
  using namespace unpack;
  auto res = make_ref<env>();

  (*res)
    (symbol("+"), builtin([](const value* args, std::size_t count) -> value {
      if(count != 2) throw std::runtime_error("wrong arg count");
      return *args[0].get<integer>() + *args[1].get<integer>();
      }))
    (symbol("-"), builtin([](const value* args, std::size_t count) -> value {
      if(count != 2) throw std::runtime_error("wrong arg count");
      return args[0].cast<integer>() - args[1].cast<integer>();
      }))
    (symbol("="), builtin([](const value* args, std::size_t count) -> value {
      if(count != 2) throw std::runtime_error("wrong arg count");
      return args[0].cast<integer>() == args[1].cast<integer>();
      }))
    
    ;
  
  return res;
};


int main(int, char**) {

  auto e = prelude();
  
  const bool debug = false;
  
  read_loop([&](std::istream& in) {
      try {
        const sexpr s = parse(in);
        // std::cout << "parsed: " << s << std::endl;
        const ast::toplevel a = ast::toplevel::check(s);
        // std::cout << "ast: " << a << std::endl;
        const value v = eval(e, a);
        std::cout << v << std::endl;
      } catch(ast::syntax_error& e) {
        std::cerr << "syntax error: " << e.what() << std::endl;
      } catch(std::runtime_error& e) {
        std::cerr << "runtime error: " << e.what() << std::endl;
      }
    });
  
  return 0;
}
