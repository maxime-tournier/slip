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


static ref<env> prelude() {
  using namespace unpack;
  auto res = make_ref<env>();
  
  (*res)
    ("+", closure(+[](const integer& lhs, const integer& rhs) -> integer {
      return lhs + rhs;
    }))
    ("-", closure(+[](const integer& lhs, const integer& rhs) -> integer {
      return lhs - rhs;
    }))
    ("=", closure(+[](const integer& lhs, const integer& rhs) -> boolean {
      return lhs == rhs;
    }))
    ;
  
  return res;
};


int main(int argc, char** argv) {

  auto re = prelude();
  auto ts = make_ref<type::state>();

  {
    using type::integer;
    using type::boolean;
    (*ts)
      ("+", integer >>= integer >>= integer)
      ("-", integer >>= integer >>= integer)
      ("=", integer >>= integer >>= boolean)            
      ;
  }

  
  const bool debug = true;
  // parser::debug::stream = &std::clog;
  
  using parser::operator+;
  static const auto program = parser::debug("prog") |= +[](std::istream& in) {
    return sexpr::parse(in);
  } >> parser::drop(parser::eof()) | parser::error<std::deque<sexpr>>("parse error");
  
  static const auto handler =
    [&](std::istream& in) {
      try {
        if(auto exprs = program(in)) {
          for(const sexpr& s : exprs.get()) {
            // std::cout << "parsed: " << s << std::endl;
            const ast::toplevel a = ast::toplevel::check(s);
            if(debug) std::cout << "ast: " << a << std::endl;

            // toplevel expression?
            if(auto e = a.get<ast::io>()->get<ast::expr>()) {
              const type::mono t = type::mono::infer(ts, *e);
              const type::poly p = ts->generalize(t);

              // TODO: cleanup variables with depth greater than current in
              // substitution
              std::cout << " : " << p;

              const value v = eval(re, *e);
              std::cout << " = " << v << std::endl;
            }
          }
          return true;
        } 
      } catch(ast::error& e) {
        std::cerr << "syntax error: " << e.what() << std::endl;
      } catch(type::error& e) {
        std::cerr << "type error: " << e.what() << std::endl;
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
