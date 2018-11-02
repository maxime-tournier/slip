#include "package.hpp"

#include <fstream>

#include "tool.hpp"
#include "infer.hpp"

package& package::def(symbol name, type::mono t, eval::value v) {
  ts->def(name, t);
  es->locals.emplace(name, v);

  return *this;
}


package::package(symbol name)
  : name(name),
    ts(make_ref<type::state>()),
    es(eval::gc::make_ref<eval::state>()) {
}


type::poly package::sig() const {
  // TODO don't expose _members
  using namespace type;
  mono res = empty;
  for(const auto& it : ts->vars->locals) {
    res = row(it.first, ts->instantiate(it.second)) |= res;
  }

  return ts->generalize(record(res));
}


eval::value package::dict() const {
  // TODO don't expose _members  
  return make_ref<eval::record>(es->locals.begin(), es->locals.end());
}


void package::exec(ast::expr e, cont_type cont) {
  const type::mono t = type::infer(ts, e);
  const type::poly p = ts->generalize(t);

  cont(p);
}


void package::exec(std::string filename) {
  // execute a file in current package
  std::ifstream in(filename);
  ast::expr::iter(in, [&](const ast::expr& e) {
      exec(e, [&](const type::poly& p) {
          eval::eval(es, e);
        });
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

std::string package::resolve(symbol name) {
  for(const std::string& prefix : package::path) {
    const std::string filename = join(prefix, name.get() + ext);
    if(exists(filename)) {
      return filename;
    }
  }
  
  throw std::runtime_error("package " + tool::quote(name.get()) + " not found");
}


static std::map<symbol, package> cache = {
  {"builtins", package::builtins()}
};

const package* package::first = &cache.begin()->second;


package package::import(symbol name) {
  const auto it = cache.find(name);
  if(it != cache.end()) return it->second;
  
  auto info = cache.emplace(name, name);

  // chain imported packages
  info.first->second.next = first;
  first = &info.first->second;
  
  const std::string filename = resolve(name);
  info.first->second.exec(filename);
  
  return info.first->second;
}


std::vector<std::string> package::path = {"", SLIP_PATH};
