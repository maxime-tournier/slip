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

#include "type.hpp"
#include "package.hpp"

#include "argparse.hpp"

#include "ir.hpp"
#include "vm.hpp"

struct history {
  const std::string filename;
  history(const std::string& filename="/tmp/slip.history")
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


template<class Func>
static void lines(const std::string& str, Func func) {
  std::stringstream ss(str);
  std::string line;
  while(std::getline(ss, line)) {
    func(line);
  }
}

static std::string print_error(const std::exception& e, bool verbose=true,
                               std::size_t level=0) {
  std::stringstream ss;

  for(std::size_t i = 0; i < level; ++i) {
    ss << "  |";
  }

  const std::string prefix = ss.str();
  
  try {
    std::rethrow_if_nested(e);
    // last one
    return e.what();
  } catch(std::exception& e) {
    // not last one
    if(verbose) {
      lines(e.what(), [&](const std::string& line) {
          std::cerr << prefix << "- " << line << std::endl;
        });
    }
    
    return print_error(e, verbose, level + 1);
  }
  
}



int main(int argc, const char** argv) {

  using namespace argparse;
  const auto parser = argparse::parser()
    .flag("debug-gc", "debug garbage collector")
    .flag("debug-tc", "debug type checking")
    .flag("debug-ast", "debug abstract syntax tree")
    .flag("time", "time evaluations")
    .flag("verbose", "be verbose")
    .flag("compile", "compile and evaluate intermediate representation")
    .flag("help", "show help")
    .argument<std::string>("filename", "file to run")
    ;
  
  const auto options = parser.parse(argc, argv);
  if(options.flag("help", false)) {
    parser.describe(std::cout);
    return 0;
  }
  
  package main("main");
  main.ts->debug = options.flag("debug-tc", false);

  const auto collect = [&] {
    const bool debug = options.flag("debug-gc", false);

    // mark main package
    eval::mark(main.es, debug);

    // mark imported packages
    for(const package* const* it = &package::first; *it; it = &(*it)->next) {
      eval::mark((*it)->es, debug);
    }

    // and sweep
    eval::gc::sweep();
  };

  
  vm::state s(1000);
  {
    s
      .def("=", vm::builtin([](const integer& lhs, const integer& rhs) {
            return lhs == rhs;
          }))
      .def("+", vm::builtin([](const integer& lhs, const integer& rhs) {
            return lhs + rhs;
          }))
      .def("-", vm::builtin([](const integer& lhs, const integer& rhs) {
            return lhs - rhs;
          }))
      .def("*", vm::builtin([](const integer& lhs, const integer& rhs) {
            return lhs * rhs;
          }))
      ;
    
  }


  using printer_type = std::function<void(std::ostream&)>;
  static const auto make_printer = [](auto value) -> printer_type {
    return [value](std::ostream& out) { out << value; };
  };

  // expression evaluator
  std::function<printer_type(ast::expr)> evaluator;
  
  if(options.flag("compile", false)) {
    evaluator = [&](ast::expr e) {
      const ir::expr c = ir::compile(e);
      return make_printer(vm::eval(&s, c));
    };
  } else {
    evaluator = [&](ast::expr e) {
      return make_printer(eval::eval(main.es, e));
    };
  }

  if(options.flag("time", false)) {
    evaluator = [evaluator](ast::expr e) {
      double duration;
      auto p = tool::timer(&duration, [&] {
          return evaluator(e);
        });
      return [p, duration](std::ostream& out) {
        p(out);
        out << "\t[time: " << duration << "s]";
      };
    };
  }

  
  static const auto reader =
    [&](std::istream& in) {
    try {
      ast::expr::iter(in, [&](ast::expr e) {
          if(options.flag("debug-ast", false)) {
            std::cout << "ast: " << e << std::endl;
          }
          main.exec(e, [&](type::poly p) {
              // TODO: cleanup substitution?
              if(auto self = e.get<ast::var>()) {
                std::cout << self->name;
              }

              const auto print = evaluator(e);
              std::cout << " : " << p << std::flush;

              print(std::cout << " = ");
              std::cout << std::endl;
            });
          collect();
        });
      return true;
    } catch(sexpr::error& e) {
      std::cerr << "parse error: " << e.what() << std::endl;
    } catch(ast::error& e) {
      std::cerr << "syntax error: " << e.what() << std::endl;
    } catch(type::error& e) {
      const std::string what = print_error(e, options.flag("verbose", true));
      std::cerr << "type error: " << what << std::endl;
    } catch(kind::error& e) {
      std::cerr << "kind error: " << e.what() << std::endl;
    } catch(std::runtime_error& e) {
      std::cerr << "runtime error: " << e.what() << std::endl;
    }
    return false;
  };

  
  if(auto filename = options.get<std::string>("filename")) {
    if(auto ifs = std::ifstream(filename->c_str())) {
      return reader(ifs) ? 0 : 1;
    } else {
      std::cerr << "io error: " << "cannot open file " << *filename << std::endl;
      return 1;
    }
  } else {
    main.exec(main.resolve("repl"));
    read_loop(reader);
  }
  
  return 0;
}
