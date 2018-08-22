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


static ref<env> prelude() {
  using namespace unpack;
  auto res = make_ref<env>();
  
  (*res)
    .def("+", closure(+[](const integer& lhs, const integer& rhs) -> integer {
      return lhs + rhs;
    }))
    .def("-", closure(+[](const integer& lhs, const integer& rhs) -> integer {
      return lhs - rhs;
    }))
    .def("=", closure(+[](const integer& lhs, const integer& rhs) -> boolean {
      return lhs == rhs;
    }))
    
    .def("nil", value::list())
    .def("cons", closure(2, [](const value* args) -> value {
      return args[0] >>= args[1].cast<value::list>();
    }))
    ;
  
  return res;
};


int main(int argc, char** argv) {

  auto re = prelude();
  auto ts = make_ref<type::state>();

  {
    using namespace type;
    using type::integer;
    using type::boolean;
    using type::list;
    using type::real;    

    // arithmetic
    (*ts)
      .def("+", integer >>= integer >>= integer)
      .def("-", integer >>= integer >>= integer)
      .def("=", integer >>= integer >>= boolean);

    // lists
    {
      const auto a = ts->fresh();
      ts->def("nil", list(a));
    }

    {
      const auto a = ts->fresh();
      ts->def("cons", a >>= list(a) >>= list(a));
    }

    // constructors
    (*ts)
      .def("integer", integer >>= integer)
      .def("real", real >>= real)      
      .def("boolean", boolean >>= boolean)
      ;    

    {
      using namespace type;
      
      const auto a = ts->fresh();
      const auto b = ts->fresh();

      const mono pair =
        make_ref<constant>(symbol("pair"), 
                           kind::term() >>= kind::term() >>= kind::term());
      ts->def("pair",
              pair(a)(b) >>= rec(row("first", a) |= row("second", a) |= empty));
    }


    {
      using namespace type;
      
      const mono f = ts->fresh(kind::term() >>= kind::term());

      const mono a = ts->fresh(kind::term());
      const mono b = ts->fresh(kind::term());      
      
      const mono functor
        = make_ref<constant>(symbol("functor"), f.kind() >>= kind::term());
      
      ts->def("functor", functor(f) >>=
              rec(row("map", f(a) >>= (a >>= b) >>= f(b)) |= empty))
        ;
    }

    
  }

  
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
