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

const bool debug = false;


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


static int with_prelude(std::function<int(ref<env> e, ref<type::state> s)> cont) {
  using namespace unpack;
  auto e = make_ref<env>();
  
  (*e)
    .def("+", closure(+[](const integer& lhs, const integer& rhs) -> integer {
          return lhs + rhs;
        }))
    .def("*", closure(+[](const integer& lhs, const integer& rhs) -> integer {
          return lhs * rhs;
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
    
    .def("isnil", closure(+[](const value::list& self) -> boolean {
          return !boolean(self);
        }))
    .def("head", closure(1, [](const value* args) -> value {
        return args[0].cast<value::list>()->head;
        }))
    .def("tail", closure(1, [](const value* args) -> value {
        return args[0].cast<value::list>()->tail;
        }))

    .def("functor", unit())
    .def("monad", unit())    

    ;

  auto s = make_ref<type::state>();

  {
    using namespace type;
    using type::integer;
    using type::boolean;
    using type::list;
    using type::real;
    
    using type::module;
    using type::record;
    
    // arithmetic
    (*s)
      .def("+", integer >>= integer >>= integer)
      .def("*", integer >>= integer >>= integer)      
      .def("-", integer >>= integer >>= integer)
      .def("=", integer >>= integer >>= boolean);

    // lists
    {
      const auto a = s->fresh();
      s->def("nil", list(a));
    }

    {
      const auto a = s->fresh();
      s->def("cons", a >>= list(a) >>= list(a));
    }

    // 
    {
      const auto a = s->fresh();
      s->def("isnil", list(a) >>= boolean);
    }

    {
      const auto a = s->fresh();
      s->def("head", list(a) >>= a);
    }

    {
      const auto a = s->fresh();
      s->def("tail", list(a) >>= list(a));
    }
    
    
    // constructors
    (*s)
      .def("boolean", module(boolean)(boolean))
      .def("integer", module(integer)(integer))
      .def("real", module(real)(real))
      ;    

    {
      // functor
      const mono f = s->fresh(kind::term() >>= kind::term());

      const mono a = s->fresh(kind::term());
      const mono b = s->fresh(kind::term());      
      
      const mono functor
        = make_ref<constant>("functor", f.kind() >>= kind::term());
      
      s->def("functor", module(functor(f))
             (record(row("map", (a >>= b) >>= f(a) >>= f(b)) |= empty)))
        ;
    }

    {
      // monad
      const mono m = s->fresh(kind::term() >>= kind::term());

      const mono a = s->fresh(kind::term());
      const mono b = s->fresh(kind::term());
      const mono c = s->fresh(kind::term());            
      
      const mono monad
        = make_ref<constant>("monad", m.kind() >>= kind::term());
      
      s->def("monad", module(monad(m))
             (record(row("bind", m(a) >>= (a >>= m(b)) >>= m(b)) |=
                     row("pure", c >>= m(c)) |=
                     empty)))
        ;
    }

    
  }

  // push a scope just in case
  return cont(scope(e), scope(s));
};


int main(int argc, char** argv) {

  // parser::debug::stream = &std::clog;

  const parser::any<sexpr> expr = sexpr::parse;

  using parser::operator*;
  static const auto program = parser::debug("prog") |=
    *expr >> parser::drop(parser::debug("eof") |= parser::eof())
    | parser::error<std::deque<sexpr>>("parse error");

  return with_prelude([&](ref<env> r, ref<type::state> s) {
      static const auto handler =
        [&](std::istream& in) {
        try {
          if(auto exprs = program(in)) {
            for(const sexpr& x : exprs.get()) {
              // std::cout << "parsed: " << s << std::endl;
              const ast::toplevel c = ast::toplevel::check(x);
              if(debug) std::cout << "ast: " << c << std::endl;

              // toplevel expression?
              if(auto e = c.get<ast::expr>()) {
                const type::mono t = type::mono::infer(s, *e);
                const type::poly p = s->generalize(t);

                // TODO: cleanup variables with depth greater than current in
                // substitution
                if(auto v = e->get<ast::var>()) std::cout << v->name;
                std::cout << " : " << p;

                const value v = eval(r, *e);
                std::cout << " = " << v << std::endl;
              }
            }
            return true;
          } 
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
    });  
}
