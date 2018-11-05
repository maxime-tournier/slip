#include "opt.hpp"

#include "ir.hpp"
#include "repr.hpp"

namespace ir {

  struct map_visitor {

    template<class Func>
    expr operator()(const expr& self, const Func& func) const {
      return func(self);
    }

    template<class Func>
    expr operator()(const ref<closure>& self, const Func& func) const {

      vector<expr> captures; captures.reserve(self->captures.size());
      for(const expr& c: self->captures) {
        captures.emplace_back(c.visit(*this, func));
      }

      expr body = self->body.visit(*this, func);

      return func(make_ref<closure>(self->argc, std::move(captures), body));
    }

    template<class Func>    
    expr operator()(const block& self, const Func& func) const {
      vector<expr> items; items.reserve(self.items.size());
      
      for(const expr& e: self.items) {
        items.emplace_back(e.visit(*this, func));
      }
      
      return func(block{std::move(items)});
    }

    template<class Func>    
    expr operator()(const ref<cond>& self, const Func& func) const {
      expr then = self->then.visit(*this, func);
      expr alt = self->alt.visit(*this, func);      

      return func(make_ref<cond>(std::move(then), std::move(alt)));
    }
  };
  
  
  // map an optimization pass recursively
  static expr map(const expr& self, expr (*pass)(const expr&)) {
    return self.visit(map_visitor(), pass);
  };


  // flatten nested blocks
  static expr flatten_blocks(const block& self) {
    vector<expr> items;

    for(const expr& e: self.items) {
      e.match([&](const expr& e) {
          items.emplace_back(e);
        },
        [&](const block& e) {
          for(const expr& sub: e.items) {
            items.emplace_back(sub);
          }
        });
    }

    return block{std::move(items)};
  }

  static expr flatten_blocks(const expr& self) {
    return self.match([&](const expr& self) {
        return self;
      }, [&](const block& self) {
        return flatten_blocks(self);
      });
  }
  
  
  // optimize expression
  expr opt(const expr& self) {
    return map(self, flatten_blocks);
  }
  
}

