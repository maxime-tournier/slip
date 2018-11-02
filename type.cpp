#include "type.hpp"

#include <vector>
#include <sstream>

#include "ast.hpp"
#include "tool.hpp"

#include "maybe.hpp"
#include "package.hpp"

#include "unify.hpp"
#include "substitution.hpp"

namespace type {

  static const auto make_constant = [](const char* name, kind::any k=kind::term()) {
    return make_ref<constant>(constant{name, k});
  };

  
  static const auto make_variable = [](std::size_t level, kind::any k=kind::term()) {
    return make_ref<variable>(variable{level, k});
  };

  // constants
  const mono unit = make_constant("unit");
  const mono boolean = make_constant("boolean");
  const mono integer = make_constant("integer");
  const mono real = make_constant("real");
  const mono string = make_constant("string");  
  

  const mono func =
    make_constant("->", kind::term() >>= kind::term() >>= kind::term());

  // state
  const mono io =
    make_constant("io", kind::term() >>= kind::term() >>= kind::term());


  // records
  const mono record =
    make_constant("record", kind::row() >>= kind::term());

  // sums
  const mono sum =
    make_constant("sum", kind::row() >>= kind::term());

  
  const mono empty = make_constant("{}", kind::row());

  
  mono ext(symbol attr) {
    static const kind::any k = kind::term() >>= kind::row() >>= kind::row();  
    static std::map<symbol, ref<constant>> table;
    const std::string name = attr.get() + std::string(":");
  
    auto it = table.emplace(attr, make_constant(name.c_str(), k));
    return it.first->second;
  }

  
  const mono ty = make_constant("type", kind::term() >>= kind::term());

  

  mono mono::operator()(mono arg) const {
    return make_ref<application>(*this, arg);
  }

  
  mono operator>>=(mono from, mono to) {
    return mono(func)(from)(to);
  }



  application::application(mono ctor, mono arg)
    : ctor(ctor),
      arg(arg) {
    const kind::any k = ctor.kind();
    if(auto c = k.get<kind::constructor>()) {
      if(*c->from != arg.kind()) {
        std::clog << poly{{}, ctor} << " :: " << ctor.kind()
                  << " " << poly{{}, arg} << " :: " << arg.kind()
                  << std::endl;
        throw kind::error("argument does not have the expected kind");
      }
    } else {
      throw kind::error("type constructor must have constructor kind");
    }
  }


  variable::variable(std::size_t level, const ::kind::any kind) :
    level(level), kind(kind) { }

  
  constant::constant(symbol name, ::kind::any k) : name(name), kind(k) { }

 
  static void quantified_kind(std::ostream& out, kind::any k) {
    k.match([&](kind::constant self) {
        out << (k == kind::term() ? "'" : k == kind::row() ? "..." : "?");
      },
      [&](kind::constructor self) {
        quantified_kind(out, *self.from);
        quantified_kind(out, *self.to);        
      });
  }

  static void bound_kind(std::ostream& out, kind::any k) {
    k.match([&](kind::constant self) {
        out << (k == kind::term() ? "!" : k == kind::row() ? "..." : "?");
      },
      [&](kind::constructor self) {
        bound_kind(out, *self.from);
        bound_kind(out, *self.to);        
      });
  }


  static void write_var(std::ostream& out, std::size_t index) {
    static constexpr std::size_t basis = 26;
    while(true) {
      out << char('a' + (index % basis));
      index /= basis;
      if(!index) return;
    }
  }

  
  struct ostream_visitor {
    
    using map_type = std::map<ref<variable>, std::size_t>;
    map_type& map;

    static const bool show_variable_kinds = false;
    
    void operator()(const ref<constant>& self, std::ostream& out,
                    const poly::forall_type& forall, bool parens) const {
      out << self->name;
    }

    
    void operator()(const ref<variable>& self, std::ostream& out,
                    const poly::forall_type& forall, bool parens) const {
      auto it = map.emplace(self, map.size()).first;
      if(forall.find(self) != forall.end()) {
        quantified_kind(out, self->kind);
      } else {
        bound_kind(out, self->kind);        
      }
    
      write_var(out, it->second);

      if(show_variable_kinds) {
        out << "("
            << std::hex << (long(self.get()) & 0xffff)
            << "::" << self->kind
            << ")";
      }
    }

    void operator()(const ref<application>& self, std::ostream& out,
                    const poly::forall_type& forall, bool parens) const {
      const kind::any k = mono(self).kind();
    
      if(self->ctor == record) {
        out << "{";
        self->arg.visit(*this, out, forall, false);
        out << "}";
        return;
      }

      if(self->ctor == sum) {
        out << "<";
        self->arg.visit(*this, out, forall, false);
        out << ">";
        return;
      }
      
      if(parens) {
        if(k == kind::term()) out << "(";
      }
    
      if(self->ctor == func) {
        // print function types in infix style
        self->arg.visit(*this, out, forall, true);
        out << " ";
        self->ctor.visit(*this, out, forall, false);
      } else if(mono(self).kind() == kind::row()) {

        // print row types: ctor is (attr: type) so we want parens
        self->ctor.visit(*this, out, forall, false);

        // skip trailing empty record type
        if(self->arg != empty) {
          out << "; ";
          self->arg.visit(*this, out, forall, false);
        }
      } else {
        // regular types
        self->ctor.visit(*this, out, forall, false);
        out << " ";

        const auto func_rhs = [&] {
          if(auto a = self->ctor.get<app>()) {
            if((*a)->ctor == func) return true;
          }
          return false;
        };

        // don't put parens on function rhs
        self->arg.visit(*this, out, forall, !func_rhs());
      }

      if(parens) {
        if(k == kind::term()) out << ")";
      }
    }
  
  };


  std::ostream& operator<<(std::ostream& out, const poly& self) {
    logger(out) << self;
    return out;
  }


  
  struct kind_visitor {
    using type = kind::any;
  
    kind::any operator()(const ref<constant>& self) const {
      return self->kind;
    }

    kind::any operator()(const ref<variable>& self) const {
      return self->kind;
    }

    kind::any operator()(const ref<application>& self) const {
      const kind::any k = self->ctor.kind();
      if(auto c = k.get<kind::constructor>()) {
        return *c->to;
      } else {
        throw std::logic_error("derp");
      }
    }
      
  };

  kind::any mono::kind() const {
    return visit(kind_visitor());
  }



  

  





  extension extension::unpack(const app& self) {
    // peel application of the form: ext(name)(head)(tail)
    const mono tail = self->arg;
    const app ctor = self->ctor.cast<app>();
    const mono head = ctor->arg;
    const cst row = ctor->ctor.cast<cst>();
    const std::string name = std::string(row->name.get());
    
    const symbol attr(name.substr(0, name.size() - 1));
    return {attr, head, tail};
  }


  




  struct logger::pimpl_type {
    ostream_visitor::map_type map;
  };

  
  logger::logger(std::ostream& out) :
    out(out), pimpl(new pimpl_type) { }

  
  logger::~logger() { }

  
  logger& logger::operator<<(const poly& p) {
    p.type.visit(ostream_visitor{pimpl->map}, out, p.forall, false);
    return *this;
  }
  

  logger& logger::operator<<(std::ostream& (*x)(std::ostream&)) {
    out << x;
    return *this;
  }
  
};
