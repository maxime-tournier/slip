#include "variant.hpp"
#include "list.hpp"

// symbol
#include <set>

// refs
#include <memory>

//
#include <map>

#include <iostream>


// base types
using boolean = bool;
using integer = long;
using real = double;

class symbol {
  const char* name;
public:
  
  explicit symbol(const std::string& name) {
    static std::set<std::string> table;
    this->name = table.insert(name).first->c_str();
  }

  const char* get() const { return name; }
  bool operator<(const symbol& other) const { return name < other.name; }
  bool operator==(const symbol& other) const { return name == other.name; }

  friend std::ostream& operator<<(std::ostream& out, const symbol& self) {
    return out << self.name;
  }
  
};


// s-expressions
struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
  using list = list<sexpr>;
};

struct env;

// values
struct lambda {
  const list<symbol> args;
  const sexpr body;
  const ref<env> scope;

  friend std::ostream& operator<<(std::ostream& out, const lambda& self) {
    return out << "#<lambda>";
  }
};

struct value : variant<real, integer, boolean, symbol, lambda, list<value> > {
  using value::variant::variant;
  using list = list<value>;
};


// environments
struct env {
  std::map<symbol, value> locals;
  ref<env> parent;

  friend ref<env> augment(const ref<env>& self, const list<symbol>& args,
                          const value::list& values) {
    auto res = std::make_shared<env>();
    res->parent = self;
    foldl(args, values, [&](const list<symbol>& args, const value& self) {
        if(!args) throw std::runtime_error("not enough args");
        res->locals.emplace(args->head, self);
        return args->tail;
      });

    return res;
  };
};



using eval_type = value (*)(const ref<env>&, const sexpr& );
static std::map<symbol, eval_type> special;

static std::runtime_error unimplemented() {
  return std::runtime_error("unimplemented");
}

static value eval(const ref<env>& e, const sexpr& expr);

static value apply(const ref<env>& e, const sexpr& func, const sexpr::list& args) {
  return eval(e, func).match<value>
    ([](value) -> value {
       throw std::runtime_error("invalid type in application");
     },
     [&](const lambda& self) {
       return eval(augment(e, self.args, map(args, [&](const sexpr& it) {
                                                     return eval(e, it);
                                                   })), self.body);
     });
}

static value eval(const ref<env>& e, const sexpr& expr) {
  return expr.match<value>
    ([](value self) { return self; },
     [&](symbol self) {
       auto it = e->locals.find(self);
       if(it == e->locals.end()) {
         throw std::runtime_error("unbound variable: " + std::string(self.get()));
       }

       return it->second;
     },
     [&](const sexpr::list& x) {
       if(!x) throw std::runtime_error("empty list in application");

       // special forms
       if(auto s = x->head.get<symbol>()) {
         auto it = special.find(*s);
         if(it != special.end()) {
           return it->second(e, x->tail);
         }
       }

       // regular apply
       return apply(e, x->head, x->tail);       
     });
}



int main(int, char**) {

  const sexpr e = symbol("michel");
  
  auto global = std::make_shared<env>();
  global->locals.emplace("michel", 14l);
  
  std::clog << e << std::endl;
  
  std::clog << eval(global, e) << std::endl;
  return 0;
}
