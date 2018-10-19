;; no "hello word (yet) sorry :/"

;; simple arithmetic
(+ 1 2)
;; : integer = 3


;; empty list
nil
;; nil : list 'a = ()

;; list size
(def (list-size (list x))
     (match x
            (nil _ 0)
            (cons self (+ 1 (list-size self.tail)))))
list-size
;; list-size : (list 'a) -> integer = #<fun>


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
(struct (functor (ctor f))
        (map (fn (a b) ((a -> b) -> (f a) -> (f b)))))
functor
;; functor : (ctor ''a) -> type (functor ''a) = ()


;; list functor instance
(def list-functor
     (new functor
          (map list-map)))
list-functor
;; list-functor : functor list = {map: #<fun>}


;; sum types: maybe
(union (maybe a)
       (none unit)
       (some a))
maybe
;; maybe : (type 'a) -> type (maybe 'a) = ()


;; convenience
(def none (new maybe
               (none ())))
none
;; none : maybe 'a = {none: ()}


;; convenience
(def (just a)
     (new maybe (some a)))
just
;; just : 'a -> maybe 'a = #<fun>


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
(struct (monad (ctor m))
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
