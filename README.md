simple prototype language with:

- s-expressions based syntax
- hindley-milner type system
- row polymorphism (scoped labels)
- first-class polymorphism with type inference
- higher-kinded types
- simple interpreter

```elisp
;; helper list functions
(def append
     (func (lhs rhs)
           (if (isnil lhs) rhs
             (cons (head lhs)
                   (append (tail lhs) rhs)))))
;; : io unit = ()

(def l1 (cons 1 (cons 2 nil)))
(def l2 (cons 3 (cons 4 nil)))

(append (cons 1 (cons 2 nil))
        (cons 3 (cons 4 nil)))
;; : list integer = (1 2 3 4)

;; list concatenation
(def concat
	 (func (xs)
		   (if (isnil xs) nil
			 (append (head xs) (concat (tail xs))))))

(concat (cons l1 (cons l2 (cons l2 nil))))
;; : list integer = (1 2 3 4 3 4)

;; list functor
(def list-map
     (func (f x)
           (if (isnil x) nil
             (cons (f (head x)) (list-map f (tail x))))))
			 
(def list-functor
	 (new functor (map list-map)))

list-functor

;; list monad
(def list-pure
     (func (x) (cons x nil)))

list-pure
;; list-pure : 'a -> list 'a = #<func>

(def list-bind
	 (func (xs f)
		   (concat (list-map f xs))))
list-bind
;; list-bind : (list 'a) -> ('a -> list 'b) -> list 'b = #<func>


(def list-monad
	 (new monad
		  (pure list-pure)
		  (bind list-bind)))
list-monad
;; list-monad : monad list = {bind: #<func>; pure: #<func>}

;; reader monad
(def reader-pure
	 (func (x)
		   (func (y) x)))
reader-pure
;; reader-pure : 'a -> 'b -> 'a = #<func>

(def reader-bind
	 (func (a f)
		   (func (e)
				 ((f (a e)) e))))
reader-bind
;; reader-bind : ('a -> 'b) -> ('b -> 'a -> 'c) -> 'a -> 'c = #<func>

(def reader-monad
	 (new monad
		  (pure reader-pure)
		  (bind reader-bind)))
reader-monad
;; reader-monad : monad 'a -> = {bind: #<func>; pure: #<func>}
```
