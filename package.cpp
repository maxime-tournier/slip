#include "package.hpp"

#include <fstream>

#include "tool.hpp"
#include "infer.hpp"

package& package::def(symbol name, type::mono t, eval::value v) {
  ts->def(name, t);
  es->def(name, v);

  return *this;
}


package::package()
  : ts(make_ref<type::state>()),
    es(make_ref<eval::state>()) {
  // TODO setup core package
}


type::poly package::sig() const {
  // TODO allow private members
  using namespace type;
  mono res = empty;
  for(const auto& it : ts->vars->locals) {
    res = row(it.first, ts->instantiate(it.second)) |= res;
  }

  return ts->generalize(record(res));
}

eval::value package::dict() const {
  eval::record result;

  for(const auto& it : es->locals) {
    result.attrs.emplace(it.first, it.second);
  }

  return result;
}


void package::exec(ast::expr e, cont_type cont) {
  const type::mono t = type::infer(ts, e);
  const type::poly p = ts->generalize(t);
  
  const eval::value v = eval::eval(es, e);
  if(cont) cont(p, v);
}

void package::exec(std::string filename) {
  // execute a file in current package
  std::ifstream in(filename);
  ast::expr::iter(in, [&](const ast::expr& e) {
    exec(e);
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

static std::string resolve(symbol name) {
  for(const std::string& prefix : package::path) {
    const std::string filename = join(prefix, name.get() + ext);
    if(exists(filename)) {
      return filename;
    }
  }
  
  throw std::runtime_error("package " + tool::quote(name.get()) + " not found");
}

package package::import(symbol name) {
  static std::map<symbol, package> cache = {
    {"builtin", builtin()}
  };
  
  const auto info = cache.emplace(name, package());
  if(info.second) {
    const std::string filename = resolve(name);
    info.first->second.exec(filename);
  }
  
  return info.first->second;
}


std::vector<std::string> package::path = {"", SLIP_PATH};
