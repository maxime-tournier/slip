#include "sexpr.hpp"

#include "../parse.hpp"

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

sexpr parse(std::istream& in) {
  using namespace parser;
  
  static const auto true_parser =
    token("true") >> [](const char*) { return pure(true); };
  
  static const auto false_parser =
    token("false") >> [](const char*) { return pure(false); };
  
  static const auto boolean_parser = true_parser | false_parser;

  static const auto separator_parser = // parser::debug("sep") |=
    noskip(chr<std::isspace>());
  
  static const auto real_parser = // parser::debug("real") |=
    value<double>();
  
  static const auto number_parser = // parser::debug("num") |= 
    real_parser >> [](real num) {
    const long cast = num;
    const sexpr value = num == real(cast) ? sexpr(cast) : sexpr(num);
    return pure(value);                     
  };


  static const auto initial_parser = chr<std::isalpha>(); 
  
  static const auto rest_parser = parser::chr<std::isalnum>();  

  static const auto symbol_parser = initial_parser >> [](char c) {
    return noskip(*rest_parser >> [c](std::deque<char>&& rest) {
        const std::string tmp = c + std::string(rest.begin(), rest.end());
        return pure(symbol(tmp));
      });
  };

  static const auto op_parser = parser::chr<matches<'+', '-', '*', '/'>>() >> [](char c) {
    return pure(symbol(std::string(1, c)));
  };
  
  static const auto attr_parser = parser::chr<matches<'@'>>() >> [](char c) { 
    return symbol_parser >> [c](symbol s) {
      return pure(symbol(c + std::string(s.get())));
    };
  };
  
  static const auto as_expr = parser::cast<sexpr>();

  static auto expr_parser = parser::any<sexpr>();

  static const auto lparen = // parser::debug("lparen") |=
    parser::token("(");
  
  static const auto rparen = // parser::debug("rparen") |=
    parser::token(")");

  static const auto exprs_parser = // parser::debug("exprs") |=
    parser::ref(expr_parser) % separator_parser;
  
  static const auto list_parser = // parser::debug("list") |=
    lparen >>= exprs_parser >> drop(rparen) 
           >> [](std::deque<sexpr>&& es) {
    return pure(make_list(es.begin(), es.end()));
  };
  
  static const auto once =
    (expr_parser
     = (boolean_parser >> as_expr)
     | (symbol_parser >> as_expr)
     | (attr_parser >> as_expr)
     | (op_parser >> as_expr)
     | number_parser
     | (list_parser >> as_expr)
     , 0); (void) once;

  parser::debug::stream = &std::clog;
  
  if(auto value = expr_parser(in)) {
    return value.get();
  }
  
  throw parse_error("parse error");
}
