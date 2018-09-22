

# parser

- extend symbol grammar
- record literals?
- variant literals?
- record line numbers somewhow (record stream_pos pairs in sexprs)

# syntax

- report line numbers in syntax errors
- type language
- module definitions

# types

- fix generalization bug in module opening

- variants, match
- better error messages
- mutable references
- sequencing/binds

- modular variants?

# eval

- garbage collection

# packages

- import path management

  
  
  
# 1ml ?

types as values: 

- parametrized types as funcs (fun ((type a)) ...)
- module signature: func( type ) -> ...
- two namespaces: one for values, one for signatures?


type constructor 
list : (type a) -> (type (list a))

module signature
list : (list a) -> (record (nil (list b)))

  - function type ok for fcp introduction, but we cannot restrict fcp
    eliminination, so no (or could we?)

module signature
list : sig (list a) (record (nil (list b)))

  - but then we need separate namespaces for modules/values (which include type
    terms)

  - do we unpack only modules in lambdas ? or types too ? or types automatically
    get a trivial module signature ?
	
	
  
  
