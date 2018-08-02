#ifndef HELPER_PARSER_HPP
#define HELPER_PARSER_HPP

#include <deque>
#include <array>
#include <type_traits>
#include <iosfwd>

#include <functional>
#include <cassert>

#include "maybe.hpp"

#include <sstream>

// a small parser combinator library, loosely based on "Monadic Parser
// Combinators" by Hutton & Meijer.

// each parser call is either:

// 1. successful and the stream has the failbit **not** set

// 2. unsuccessful and the stream state (pos/rdstate) must be restored as if
// before the call. use the stream_state RAII for this.

namespace parser {

namespace {

template<class Parser>
using result_type = typename detail::require_maybe<
    typename std::result_of<Parser(std::istream&)>::type
    >::type;

template<class Parser>
using value_type = typename result_type<Parser>::value_type;


// RAII for saving/restoring/discarding stream state
class stream_state {
    std::istream* stream;
public:
    const std::ios::iostate state;
    const std::ios::pos_type pos;
    
    inline stream_state(std::istream& stream)
        : stream(&stream),
          state(stream.rdstate()),
          pos(stream.good() ? stream.tellg() :  std::ios::pos_type() ){

    }

    inline ~stream_state() {
        if(!stream) return;
        stream->clear(state);
        if(stream->good()) {
            stream->seekg(pos);
        }
    }

    inline stream_state(stream_state&& other)
        : stream(other.stream),
          state(other.state),
          pos(other.pos) {
        other.stream = nullptr;
    }

    inline void discard() { stream = nullptr; }
};


////////////////////////////////////////////////////////////////////////////////
// parser combinators

// type erasure
template<class T>
using any = std::function< maybe<T> (std::istream& ) >;


// reference
template<class Parser>
struct ref_type {
    const Parser& parser;

    result_type<Parser> operator()(std::istream& in) const {
        return parser(in);
    }
};

template<class Parser>
static ref_type<Parser> ref(const Parser& parser) { return {parser}; }


// monadic bind (parser sequencing with binding)
template<class Parser, class Func>
struct bind_type {
    const Parser parser;
    const Func func;

    using func_result_type = typename std::result_of<Func(value_type<Parser>)>::type;
    
    result_type<func_result_type> operator()(std::istream& in) const {
        stream_state backup(in);
        assert(!in.fail());
        
        if(auto value = parser(in)) {
            if(auto res = func(std::move(value.get()))(in)) {
                backup.discard();
                return std::move(res.get());
            }
        }
        return {};
    }
};

template<class Parser, class Func, class = value_type<Parser> >
static bind_type<Parser, Func> operator>>(const Parser& parser, const Func& func) {
    return {parser, func};
}


// monadic pure (lift value as a parser)
template<class T>
struct pure_type {
    const T value;

    maybe<T> operator()(std::istream& ) const { return value; }
};

template<class T>
static pure_type< typename std::decay<T>::type> pure(T&& value = {}) {
    return {std::forward<T>(value)};
}

// monadic zero
template<class T>
struct fail {
    maybe<T> operator()(std::istream& ) const { return {}; }
};

// functor map
template<class Parser, class Func>
struct map_type {
    const Parser parser;
    const Func func;

    using type = typename std::result_of<Func(value_type<Parser>)>::type;
    maybe<type> operator()(std::istream& in) const {
        return map(parser(in), func);
    }
    
};

template<class Parser, class Func>
static map_type<Parser, Func> map(const Parser& parser, const Func& func) {
    return {parser, func};
}



// sequencing without binding
template<class Parser>
struct then_type {
    const Parser parser;
    
    template<class T>
    Parser operator()(const T&) const { return parser; }
};

template<class Parser>
static then_type<Parser> then(const Parser& parser) { return {parser}; }

template<class LHS, class RHS>
struct sequence_type {
    using impl_type = bind_type<LHS, then_type<RHS>>;
    const impl_type impl;

    sequence_type(const LHS& lhs, const RHS& rhs)
        : impl( lhs >> then(rhs) ) {

    }
    
    result_type<RHS> operator()(std::istream& in) const {
        return impl(in);
    }
    
};

template<class LHS, class RHS, class = value_type<LHS>, class = value_type<RHS>>
static sequence_type<LHS, RHS> operator>>=(const LHS& lhs, const RHS& rhs) {
    return {lhs, rhs};
}


// sequencing with result drop
template<class Parser>
struct drop_type {
    const Parser parser;

    template<class T>
    sequence_type<Parser, pure_type<typename std::decay<T>::type>>
    operator()(T&& value) const {
        return parser >>= pure(std::forward<T>(value));
    }
};

template<class Parser>
static drop_type<Parser> drop(const Parser& parser) { return {parser}; }


// sequencing with result cast
template<class U>
struct cast {
    template<class T>
    pure_type<U> operator()(T&& value) const {
        return pure<U>(std::forward<T>(value));
    }
};


// parser either LHS or RHS (usual short-circuit applies)
// TODO use coproduct type for values and relax check
template<class LHS, class RHS>
struct coproduct_type {
    const LHS lhs;
    const RHS rhs;

    static_assert(std::is_same< result_type<LHS>, result_type<RHS> >::value,
                  "parser value types must agree");
    
    result_type<LHS> operator()(std::istream& in) const {
        stream_state backup(in);
        if(auto res = lhs(in)) {
            backup.discard();
            return std::move(res.get());
        } else if(auto res = rhs(in)) {
            backup.discard();            
            return std::move(res.get());
        } else {
            return {};
        }
    }
};


template<class LHS, class RHS, class = value_type<LHS>, class = value_type<RHS>>
static coproduct_type<LHS, RHS> operator|(const LHS& lhs, const RHS& rhs) {
    return {lhs, rhs};
}


// kleene star
template<class Parser>
struct kleene_type {
    const Parser parser;

    using type = std::deque< value_type<Parser> >;
    maybe<type> operator()(std::istream& in) const {
        type res;
        while(auto value = parser(in)) {
            res.emplace_back(std::move(value.get()));
        }
        return res;
    }
};

template<class Parser, class = value_type<Parser> >
static kleene_type<Parser> operator*(const Parser& parser) {
    return {parser};
}


// (non-empty) kleene star
template<class Parser>
struct plus_type {
    const Parser parser;

    using type = std::deque< value_type<Parser> >;
    maybe<type> operator()(std::istream& in) const {
      const auto impl = ref(parser) >> [&](value_type<Parser>&& first) {
        return *ref(parser) >> [&](std::deque<value_type<Parser>>&& rest) {
          rest.emplace_front(std::move(first));
          return pure(rest);
        };
      };
      
      return impl(in);
    }
};

template<class Parser, class = value_type<Parser> >
static plus_type<Parser> operator+(const Parser& parser) {
    return {parser};
}
  

  

// repetition 
template<std::size_t N, class Parser>
struct repeat_type {
    const Parser parser;

    using type = std::array<value_type<Parser>, N>;
    maybe<type> operator()(std::istream& in) const {
        stream_state backup(in);
        type res;

        for(std::size_t i = 0; i < N; ++i) {
            if(auto ith = parser(in)) {
                res[i] = std::move(ith.get());
            } else {
                return {};
            }
        }

        return res;
    }
};

template<std::size_t N, class Parser>
static repeat_type<N, Parser> repeat(const Parser& parser) { return {parser}; }


template<class T, class Exception = std::runtime_error>
struct error : Exception {
    using Exception::Exception;
    
    maybe<T> operator()(std::istream&) const {
        throw *this;
    }
};


// *non-empty* list with separator
template<class Parser, class Separator>
struct list {
    const Parser parser;
    const Separator separator;

    using type = std::deque<value_type<Parser>>;
    maybe<type> operator()(std::istream& in) const {
      const auto impl = ref(parser)
        >> [&](value_type<Parser>&& first) {
             return *(ref(separator) >> then(ref(parser)))
               >> [&](type&& rest) {
                    rest.emplace_front(std::move(first));
                    return pure(std::move(rest));
                  };
           };
      
        return impl(in);
    }
};

template<class Parser, class Separator,
         class = value_type<Parser>, class = value_type<Separator>>
static list<Parser, Separator> operator%(const Parser& parser,
                                         const Separator& separator) {
    return {parser, separator};
};

struct stream_format {
    std::istream* in;
    const std::istream::fmtflags fmt;
    inline stream_format(std::istream& in)
        : in(&in), fmt(in.flags()) { }

    inline ~stream_format() {
        if(in) in->flags(fmt);
    }
};

// unformatted parsing
template<class Parser>
struct noskip_type {
    const Parser parser;

    result_type<Parser> operator()(std::istream& in) const {
        const stream_format backup(in);
        return parser(in >> std::noskipws);
    }
};


template<class Parser>
static noskip_type<Parser> noskip(const Parser& parser) {
    return {parser};
}


////////////////////////////////////////////////////////////////////////////////
// actual parsers

// formatted for first char, unformatted for the rest
struct token {
    inline token(const char* value) : expected(value) { }
    const char* const expected;

    inline maybe<const char*> operator()(std::istream& in) const {
        stream_state backup(in);
        const auto fmt = in.flags();

        // first read is formatted
        // in >> std::skipws;
        for(const char* c = expected; *c; ++c) {
            char i = 0;
            if(!(in >> i) || i != *c) {
                in.flags(fmt); // restore flags
                return {};
            }

            // next reads are unformatted
            if(c == expected) in >> std::noskipws;
        }

        backup.discard();
        in.flags(fmt);    // restore flags    
        return expected;
    }
};


// parse a value using standard stream input
template<class T>
struct value {
    maybe<T> operator()(std::istream& in) const {
      stream_state backup(in);
      T res;

      if(in >> res) {
        backup.discard();
        return res;
      } else {
        return {};
      }
    }
};

// parse true if the next read would yield eof
struct eof {
    inline maybe<bool> operator()(std::istream& in) const {
        stream_state backup(in);
        char c;
        if( !(in >> c) && in.eof() ) return true;
        return {};
    }
};


// optional parser. value type must be default constructible in case parse
// failed
template<class Parser>
struct option_type {
    const Parser parser;

    result_type<Parser> operator()(std::istream& in) const {
        if(auto res = parser(in)) {
            return res;
        }
        return {{}};
    }
};

template<class Parser, class = result_type<Parser>>
static option_type<Parser> operator~(const Parser& parser) { return {parser}; }


// character parser from predicate. warning: parsing is **formatted** unless
// explicitely disabled with 'noskip'
template<int (*f) (int)>
struct chr {
    
    maybe<char> operator()(std::istream& in) const {
        stream_state backup(in);
        char c;
        if((in >> c) && f(c)) {
          backup.discard();
          return c;
        }
        return {};
    };
    
};


// TODO character range/set/predicates
template<class Parser>
struct debug_type {
    const Parser parser;
    const char* const trace;
    
    result_type<Parser> operator()(std::istream& in) const;
};


namespace {
struct debug {
    const char* trace;
    inline debug(const char* trace) : trace(trace) { }

    template<class Parser>
    inline debug_type<Parser> operator|=(const Parser& parser) const {
        return {parser, trace};
    }

    static std::ostream* stream;
    static std::size_t depth;
};

std::ostream* debug::stream = nullptr;
std::size_t debug::depth = 0;
}


static std::string inline istream_slice(std::istream& in,
                                        std::istream::streampos first,
                                        std::istream::streampos last) {
  if(first == -1) return {};

  in.seekg(first);
  
  if(last == -1) {
    std::stringstream ss;
    in >> ss.rdbuf();
    return ss.str();
  }

  const std::size_t size = last - first;
  std::string buffer;
  buffer.resize(size);

  in.read(&buffer[0], size);
  return buffer;
}
  
template<class Parser>
inline result_type<Parser> debug_type<Parser>::operator()(std::istream& in) const {
    const std::size_t depth = debug::depth++;
    const std::istream::streampos prev = in.eof() ?
      std::istream::streampos(-1) : in.tellg();
    
    const auto indent = [depth] () -> std::ostream& {
        assert(debug::stream);
        for(std::size_t i = 0; i < depth; ++i) {
          *debug::stream << "  ";
        }
        return *debug::stream;
    };
    
    if(debug::stream) {
        indent() << ">> " << trace << std::endl;
    }

    assert(!in.fail());
    auto res = parser(in);
    assert(!in.fail());
    
    if(debug::stream) {
      const stream_state backup(in);
      if(res) {
        const std::istream::streampos curr = in.eof() ?
          std::istream::streampos(-1) : in.tellg();
        
        indent() << "<< " << trace
                 << " success \"" << istream_slice(in, prev, curr)
                 << "\"" << std::endl;
        } else {
            std::string next;
            in >> next;
            indent() << "<< " << trace
                     << " failed on \"" << next << "\"" << std::endl;
        }
    }

    --debug::depth;

    return res;
}





// adaptor to standard c++ parsing api
template<class Parser>
static std::istream& parse(value_type<Parser>& self, const Parser& parser,
                           std::istream& in) {
    if(auto res = parser(in)) {
        self = std::move(res.get());
    } else {
        in.setstate(std::ios::failbit);
    }

    return in;
}

}
}




#endif
