#ifndef SLAP_AST_HPP
#define SLAP_AST_HPP

#include <functional>

#include "ref.hpp"
#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "base.hpp"


struct sexpr;

namespace ast {

  // expressions
  template<class T>
  struct lit {
    const T value;
  };


  template<class T>
  static lit<T> make_lit(T value) { return {value}; }

  struct expr;

  static inline ref<expr> make_expr(const expr& e) { return make_ref<expr>(e); }
  static inline ref<expr> make_expr(expr&& e) { return make_ref<expr>(std::move(e)); }  
  
  struct app {
    app(const expr& func, const list<expr>& args)
      : func(make_ref<expr>(func)),
        args(args) { }
    
    const ref<expr> func;
    const list<expr> args;
  };


  struct abs {
    struct typed;
    struct arg;
    
    const list<arg> args;
    const ref<expr> body;

    abs(const list<arg>& args, const expr& body)
      : args(args), body(make_expr(body)) { }

  };

  struct bind;
  
  struct let {
    let(const list<bind>& defs, const expr& body)
      : defs(defs), body(make_expr(body)) { }
    
    const list<bind> defs;
    const ref<expr> body;
  };

  struct var {
    const symbol name;
  };


  // conditionals
  struct cond {
    cond(const expr& test,
         const expr& conseq,
         const expr& alt)
      : test(make_expr(test)),
        conseq(make_expr(conseq)),
        alt(make_expr(alt)) { }
    
    const ref<expr> test;
    const ref<expr> conseq;
    const ref<expr> alt;
  };


  // records
  struct record {
    struct attr;

    const list<attr> attrs;
  };


  // attribute selection
  struct sel {
    // note: record attributes have the same restriction as variable names since
    // they can be imported
    const var id;
  };

  // value injection
  struct inj {
    const var id;
  };
  
  // pattern matching
  struct match {
    struct handler;
    const list<handler> cases;
    const ref<expr> fallback;
  };
  

  // module definition
  struct module {
    const var id;

    const list<abs::arg> args;
    const list<record::attr> attrs;
    
    enum type {product, coproduct} type;
  };
  

  // module packing
  struct make {
    const ref<expr> type;
    const list<record::attr> attrs;
  };

  // definition (modifies current environment)
  struct def {
    const var id;
    const ref<expr> value;
    def(var id, const expr& value);
  };

  // sequencing
  struct io;
  
  struct seq {
    const list<io> items;
  };

  // unpack a record into the environment
  struct use {
    const ref<expr> env;
    use(const expr& env) : env(make_expr(env)) { }
    
    // const ref<expr> body;
    // use(const expr& env, const expr& body)
    //   : env(make_expr(env)),
    //     body(make_expr(body)) { }
    
  };

  // load and add result to the environment
  struct import {
    const symbol package;
  };
  
  
  struct expr : variant<lit<unit>,
                        lit<boolean>,
                        lit<integer>,
                        lit<real>,
                        var, abs, app, let,
                        cond,
                        record, sel,
                        match, inj,
                        make, use,
                        import,
                        def, seq,
                        module> {
    using expr::variant::variant;
    using list = list<expr>;
    
    friend std::ostream& operator<<(std::ostream& out, const expr& self);

    static expr check(const sexpr& e);
    static expr toplevel(const sexpr& e);
    
    static void iter(std::istream& in, std::function<void(expr)> cont);
  };

  
  struct abs::typed {
    const expr type;
    const var id;
  };

  
  struct abs::arg : variant<var, typed> {
    using arg::variant::variant;
    using list = list<arg>;
    symbol name() const;
  };  

  
  struct record::attr {
    using list = list<attr>;
    // note: record attributes have the same restriction as variable names since
    // they can be imported
    const var id;
    const expr value;
  };

  
  // note: match handlers introduce a variable binding the same ways as
  // functions do
  struct match::handler {
    using list = list<handler>;
    const var id;
    const abs::arg arg;
    const expr value;
  };

  
  // monadic binding in a sequence
  struct bind {
    const var id;
    const expr value;
  };
  
  // stateful computations
  struct io : variant<bind, expr>{
    using io::variant::variant;

    static io check(const sexpr& e);
  };

  struct error : std::runtime_error {
    using runtime_error::runtime_error;
  };


  namespace kw {
    // reserved keywords
    extern symbol abs,
      let,
      seq,
      def,
      cond,
      record,
      match,
      make,
      bind,
      use,
      import,
      product,
      coproduct,
      wildcard
      ;
  }
}


#endif
