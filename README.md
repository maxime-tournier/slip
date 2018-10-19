## what

simple prototype language with:

- s-expressions based syntax
- hindley-milner-based type system
- row polymorphism (scoped labels)
- (open) sum types + pattern matching
- first-class polymorphism with type inference
- higher-kinded types
- minimal package imports
- cryptic error messages :-/
- minimal interpreter

## how

the main parts:

1. vanilla hindley-milner
2. row polymorphism à la [Leijen](https://www.microsoft.com/en-us/research/publication/extensible-records-with-scoped-labels/), with both records and sums
3. first-class polymorphism à
  la [Jones](http://web.cecs.pdx.edu/~mpj/pubs/fcp.html) *i.e.* function
  variables introduce FCP + give up first-class data constructors. the only data
  constructors are module constructors
4. parametrized signatures also à
  la [Jones](http://web.cecs.pdx.edu/~mpj/pubs/paramsig.html), with
  universal quantifiers only
5. type/module/value language à
  la [Rossberg](https://people.mpi-sws.org/~rossberg/1ml/), but trying hard to
  stick to rank-1 types (and use FCP when necessary)

1, 2 are pretty much standard. 3 is implemented using modules as the only data
constructors, with associated parametrized signatures similar to 4. for
instance:

```elisp
(new functor
   (map list-map))
```

this boxes the record type `forall a, b, f. {map: (a -> b) -> (f a) -> (f b)}`
as module type `forall f. functor f`. module signatures are associated with
module type constructors (here `functor`) and are used to unbox module values.
in this case the signature is:

`forall a, b, f. (functor f) -> {map: (a -> b) -> (f a) -> (f b)}`

5 is used because of me being too lazy to implement a separate language for
types and terms. so I tried to use the term language as much as possible.

```elisp
(def (list-size (list x))
     (match x
            (nil _ 0)
            (cons self (+ 1 (list-size self.tail)))))
```

the the above example, `list` is actually a value:

```elisp
list
;; list : (type 'a) -> type (list 'a) = #<fun>
```

the right-most `list` is the *type* constructor `list`, which is here reified as
the *value* `list`. the *value* is used for function parameter type annotations
as shown above. the *type* constructor `list` is associated with the following
signature:

`forall a. (list a) -> <cons: {head: a; tail: list a}; nil: list a>`

which makes it possible to automatically unbox values of type `list a` at
function applications (automatic coercion), in this case for
pattern matching.

## why

- to discover/understand how functional languages work
  
- to have a simple basis for trying out advanced features like lifetime
  polymorphism, linear type systems, etc

- for the fun!!1

## example 

```elisp
;; no "hello word (yet) sorry :/"

;; simple arithmetic
(+ 1 2)
;; : integer = 3


;; empty list
nil
;; nil : list 'a = ()


;; list concatenation, pattern matching
(def (concat lhs rhs)
     (match lhs
            (cons self (cons self.head (concat self.tail rhs)))
            (nil _ rhs)))
concat
;; concat : <cons: {head: 'a; ...b}; nil: 'c> -> (list 'a) -> list 'a = #<fun>


;; build a list containing an integer range
(def (range start end)
     (if (= start end) nil
       (cons start (range (+ start 1) end))))
range
;; range : integer -> integer -> list integer = #<fun>


;; some test list
(def data (concat (range 0 10) (range 0 3)))
data
;; data : list integer = (0 1 2 3 4 5 6 7 8 9 0 1 2)


;; list map
(def (list-map f (list x))
     (match x
            (nil _ nil)
            (cons self (cons (f self.head) (list-map f self.tail)))))
list-map
;; list-map : ('a -> 'b) -> (list 'a) -> list 'b = #<fun>


;; doubling our test list
(list-map (fn (x) (* 2 x)) data)


;; functor module definition
(module (functor (ctor f))
        (map (fn (a b) ((a -> b) -> (f a) -> (f b)))))
functor
;; functor : (ctor ''a) -> type (functor ''a) = ()


;; list functor instance
(def list-functor
     (new functor
          (map list-map)))
list-functor
;; list-functor : functor list = {map: #<fun>}


;; maybe map
(def (maybe-map f (maybe a))
     (match a
            (none _ none)
            (some self (just (f self)))))
maybe-map
;; maybe-map : ('a -> 'b) -> (maybe 'a) -> maybe 'b = #<fun>


;; maybe functor instance
(def maybe-functor
     (new functor
          (map maybe-map)))

maybe-functor
;; maybe-functor : functor maybe = {map: #<fun>}


;; omg monads!!11
(module (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (>>= (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))
monad
;; monad : (ctor ''a) -> type (monad ''a) = ()


;; maybe bind
(def (maybe-bind (maybe a) f)
     (match a
            (none _ none)
            (some a (f a))))
maybe-bind
;; maybe-bind : (maybe 'a) -> ('a -> maybe 'b) -> maybe 'b = #<fun>


;; maybe monad instance
(def maybe-monad
     (new monad
          (pure just)
          (>>= maybe-bind)))

maybe-monad
;; maybe-monad : monad maybe = {>>=: #<fun>; pure: #<fun>}


;; reader pure
(def (reader-pure x)
     (fn (y) x))
reader-pure
;; reader-pure : 'a -> 'b -> 'a = #<fun>


;; reader bind
(def (reader-bind a f)
     (fn (y)
         (f (a y) y)))
reader-bind
;; reader-bind : ('a -> 'b) -> ('b -> 'a -> 'c) -> 'a -> 'c = #<fun>


;; reader monad
(def reader-monad
     (new monad
          (pure reader-pure)
          (>>= reader-bind)))
reader-monad
;; reader-monad : monad 'a -> = {>>=: #<fun>; pure: #<fun>}
```

## TODO

- improve error messages
- do notation
- state monad, mutable refs, etc


