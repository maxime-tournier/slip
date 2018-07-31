#include "sexpr.hpp"

#include "../parse.hpp"

namespace {

  template<char c>
  static int equals(int x) {
    return x == c;
  }
}

sexpr parse(std::istream& in) {
  using parser::pure;
  
  static const auto true_parser =
    parser::token("true") >> [](const char*) { return pure(true); };
  
  static const auto false_parser =
    parser::token("false") >> [](const char*) { return pure(false); };
  
  static const auto boolean_parser = true_parser | false_parser;

  static const auto separator_parser = // parser::debug("sep") |=
    noskip(parser::chr<std::isspace>());
  
  static const auto real_parser = // parser::debug("real") |=
    parser::value<double>();
  
  static const auto number_parser = // parser::debug("num") |= 
    real_parser >> [](real num) {
    const long cast = num;
    const sexpr value = num == real(cast) ? cast : num;
    return pure(value);                     
  };


  static const auto initial_parser = parser::chr<std::isalpha>(); 
  
  static const auto rest_parser = parser::chr<std::isalnum>();  
  
  static const auto symbol_parser =
    initial_parser >>
    [](char c) {
      return noskip(*rest_parser >>
                    [c](std::deque<char>&& rest) {
                      const std::string tmp =
                        c + std::string(rest.begin(), rest.end());
                      
                      return pure(symbol(tmp));
                    });
    };

  static const auto attr_parser = parser::chr<equals<'@'>>()
    >> [](char c) {
         return symbol_parser
           >> [c](symbol s) {
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
    (expr_parser = // parser::debug("expr") |=
     (boolean_parser >> as_expr)
     | (symbol_parser >> as_expr)
     | (attr_parser >> as_expr)
     | number_parser
     | (list_parser >> as_expr)
     , 0); (void) once;

  parser::debug::stream = &std::clog;
  
  if(auto value = expr_parser(in)) {
    return value.get();
  }
  
  throw std::runtime_error("parse error");
}
