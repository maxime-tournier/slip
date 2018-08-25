

# parser

- extend symbol grammar
- record literals?
- variant literals?
- record line numbers somewhow
- qualified names foo.bar.baz -> (:baz (:bar foo))

# syntax

- record line numbers somewhow
- type language
- type definitions

# types

- importing/exporting modules
- variants, match
- precise error messages
- mutable variables
- sequencing/binds

- modular variants?

# eval

- importing/exporting modules


# modules

- (use `symbol`) : unwrap module and populate environment with contents
  : io unit
  
- (import `symbol`) : load file and assign last result to variable
  : io unit
  
