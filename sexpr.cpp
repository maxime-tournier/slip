#include "sexpr.hpp"

#include "parse.hpp"
#include <algorithm>

namespace {

  template<char ... c>
  static int matches(int x) {
    static const char data[] = {c..., 0};

    for(const char* i = data; *i; ++i) {
      if(*i == x) return true;
    }

    return false;
  }
  
}


template<class T>
static const std::ios::pos_type try_parse(std::istream& in) {
  const parser::stream_state backup(in);
  T t;
  in >> t;
  return parser::stream_state(in).pos;
}


maybe<sexpr> sexpr::parse(std::istream& in) {
  using namespace parser;
  
  static const auto separator_parser = // parser::debug("sep") |=
    noskip(chr<std::isspace>());

  static const auto integer_parser = // parser::debug("real") |=
    value<integer>();
  
  static const auto real_parser = // parser::debug("real") |=
    value<double>();

  static const auto semicolon = chr<matches<';'>>();
  static const auto endl = chr<matches<'\n'>>();
  
  static const auto comment_parser = debug("comment") |=
    semicolon >> then(noskip(*!endl));
  
  static const auto skip_parser = debug("skip") |=
    comment_parser;
  
  static const auto as_expr = parser::cast<sexpr>();
  
  static const auto number_parser = [](std::istream& in) {
    const auto pr = try_parse<real>(in);
    const auto pi = try_parse<integer>(in);    

    if(pr == pi) return (integer_parser >> as_expr)(in);
    return (real_parser >> as_expr)(in);
  };


  static const auto initial_parser = chr<std::isalpha>() | chr<matches<'_'>>(); 
  
  static const auto rest_parser = chr<std::isalnum>() | chr<matches<'-', '_'>>();  

  static const auto symbol_parser = initial_parser >> [](char c) {
    return noskip(*rest_parser >> [c](std::deque<char>&& rest) {
        const std::string tmp = c + std::string(rest.begin(), rest.end());
        return pure(symbol(tmp));
      });
  };

  static const auto op_parser =
    chr<matches<'+', '-', '*', '/', '=', '<', '>', '%' >>() >> [](char c) {
    return pure(symbol(std::string(1, c)));
  } | (token("!=") | token("<=") | token(">=")) >> [](const char* s) {
    return pure(symbol(s));
  };
  
  
  static const auto attr_parser = chr<matches<':'>>() >> [](char c) { 
    return symbol_parser >> [c](symbol s) {
      return pure(symbol(c + std::string(s.get())));
    };
  };
  
  static auto expr_parser = any<sexpr>();

  static const auto lparen = // debug("lparen") |=
    token("(");
  
  static const auto rparen = // debug("rparen") |=
    skip(skip_parser, token(")"));

  static const auto exprs_parser = // debug("exprs") |=
    parser::ref(expr_parser) % separator_parser;
  
  static const auto list_parser = // debug("list") |=
    lparen >>= exprs_parser >> drop(rparen) 
           >> [](std::deque<sexpr>&& es) {
    return pure(make_list(es.begin(), es.end()));
  };
  
  static const auto once =
    (expr_parser =
     (debug("expr") |=
      skip(skip_parser,
           (symbol_parser >> as_expr)
           | (attr_parser >> as_expr)
           | (op_parser >> as_expr)
           | number_parser
           | (list_parser >> as_expr)))
      , 0); (void) once;

  // debug::stream = &std::clog;
  return expr_parser(in);
}


void sexpr::iter(std::istream& in, std::function<void(sexpr)> cont) {
  using namespace parser;

  using exprs_type = std::deque<sexpr>;
  
  static const auto program_parser = debug("prog") |=
    kleene(parse) >> drop(debug("eof") |= eof())
    | parser::error<exprs_type>("parse error");
  
  (program_parser >> [cont](exprs_type es) {
    std::for_each(es.begin(), es.end(), cont);
    return pure(unit());
  })(in);
  
}

