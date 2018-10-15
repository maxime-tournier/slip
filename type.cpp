#include "type.hpp"

#include <vector>
#include <sstream>

#include "ast.hpp"
#include "tool.hpp"

#include "maybe.hpp"
#include "package.hpp"

#include "unify.hpp"

namespace type {

  static constexpr bool debug = false;

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
  

  const mono func =
    make_constant("->", kind::term() >>= kind::term() >>= kind::term());

  const mono io =
    make_constant("io", kind::term() >>= kind::term());
  
  // records
  const mono record =
    make_constant("record", kind::row() >>= kind::term());

  const mono empty = make_constant("{}", kind::row());
  
  mono ext(symbol attr) {
    static const kind::any k = kind::term() >>= kind::row() >>= kind::row();  
    static std::map<symbol, ref<constant>> table;
    const std::string name = attr.get() + std::string(":");
  
    auto it = table.emplace(attr, make_constant(name.c_str(), k));
    return it.first->second;
  }

  const mono ty = make_constant("type", kind::term() >>= kind::term());
  
  
  state& state::def(symbol name, mono t) {
    if(!vars->locals.emplace(name, generalize(t)).second) {
      throw error("redefined variable " + tool::quote(name.get()));
    }
  
    return *this;
  }
  


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
  

  struct ostream_visitor {
    using type = void;
  
    using map_type = std::map<ref<variable>, std::size_t>;
    map_type& map;

    void operator()(const ref<constant>& self, std::ostream& out,
                    const poly::forall_type& forall, bool parens) const {
      out << self->name;
    }

    void operator()(const ref<variable>& self, std::ostream& out,
                    const poly::forall_type& forall, bool parens) const {
      auto it = map.emplace(self, map.size()).first;
      if(forall.find(self) != forall.end()) {
        out << "'";
      } else {
        out << "!";
      }
    
      out << char('a' + it->second);

      static const bool extra = false;
      if(debug && extra) {
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
        // skip record constructor altogether since row types have {} around
        self->arg.visit(*this, out, forall, true);
        return;
      }

      if(parens) {
        if(k == kind::row()) out << "{";
        else out << "(";
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
        if(k == kind::row()) out << "}";
        else out << ")";
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



  

  


  // polytype instantiation
  struct instantiate_visitor {
    using type = mono;
  
    using map_type = std::map<ref<variable>, ref<variable>>;
    const map_type& map;
  
    mono operator()(const ref<constant>& self) const {
      return self;
    }

    mono operator()(const ref<variable>& self) const {
      auto it = map.find(self);
      if(it != map.end()) {
        return it->second;
      } else {
        return self;
      }
    }

    mono operator()(const ref<application>& self) const {
      return self->ctor.visit(*this)(self->arg.visit(*this));
    }
  
  };


  mono state::instantiate(const poly& self) const {
    // associate quantified variables with fresh variables
    instantiate_visitor::map_type map;
    for(const ref<variable>& it : self.forall) {
      // assert(it->level <= level);
      map.emplace(it, make_ref<variable>( variable{level, it->kind} ));
    }

    // instantiate
    return self.type.visit(instantiate_visitor{map});
  }




  // type state
  state::state()
    : level(0),
      vars(make_ref<vars_type>()),
      sigs(make_ref<sigs_type>()),
      sub(make_ref<substitution>()) {

  }


  state::state(const ref<state>& parent)
    : level(parent->level + 1),
      vars(make_ref<vars_type>(parent->vars)),
      sigs(parent->sigs),
      sub(parent->sub)
  {

  }


  ref<variable> state::fresh(kind::any k) const {
    return make_variable(level, k);
  }
  


  // generalize
  struct generalize_visitor {
    using type = void;
  
    const std::size_t level;
  
    template<class OutputIterator>
    void operator()(const ref<constant>&, OutputIterator) const { }

    template<class OutputIterator>
    void operator()(const ref<variable>& self, OutputIterator out) const {
      // generalize if variable is deeper than current level
      if(self->level >= level) {
        *out++ = self;
      }
    }

    template<class OutputIterator>
    void operator()(const ref<application>& self, OutputIterator out) const {
      self->ctor.visit(*this, out);
      self->arg.visit(*this, out);
    }
  
  };


  poly state::generalize(const mono& t) const {
    poly::forall_type forall;
    const mono s = substitute(t);
    s.visit(generalize_visitor{level}, std::inserter(forall, forall.begin()));
    return poly{forall, s};
  }


  struct substitute_visitor {
    using type = mono;
  
    mono operator()(const ref<variable>& self, const state& s) const {
      auto it = s.sub->find(self);
      if(it == s.sub->end()) return self;
      assert(it->second != self);
      return s.substitute(it->second);
    }

    mono operator()(const ref<application>& self, const state& s) const {
      return s.substitute(self->ctor)(s.substitute(self->arg));
    }

    mono operator()(const ref<constant>& self, const state&) const {
      return self;
    }
  
  };

  mono state::substitute(const mono& t) const {
    return t.visit(substitute_visitor(), *this);
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


  








  void state::unify(mono from, mono to) {
    logger log(std::clog);
    unify_terms(this, from, to, debug ? &log : nullptr);
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
