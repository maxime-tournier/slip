#include "package.hpp"

#include <fstream>

package& package::def(symbol name, type::mono t, eval::value v) {
  ts->def(name, t);
  es->def(name, v);

  return *this;
}


package::package(resolver_type resolver)
  : ts(make_ref<type::state>(resolver)),
    es(make_ref<eval::state>()) {
  // TODO setup core package
}


type::poly package::sig() const {
  // TODO allow private members
  using namespace type;
  mono res = empty;
  for(const auto& it : ts->locals) {
    res = row(it.first, ts->instantiate(it.second)) |= res;
  }

  return ts->generalize(record(res));
}

void package::exec(ast::expr e, cont_type cont) {
  const type::mono t = type::infer(ts, e);
  const type::poly p = ts->generalize(t);
  
  const eval::value v = eval::expr(es, e);
  cont(p, v);
}

void package::exec(std::string filename) {
  std::ifstream in(filename);
  ast::expr::iter(in, [&](const ast::expr& e) {
    exec(e);
  });
}
