
;; helper list functions
(def append
     (func (lhs rhs)
           (if (isnil lhs) rhs
             (cons (head lhs)
                   (append (tail lhs) rhs)))))

(def l1 (cons 1 (cons 2 nil)))
(def l2 (cons 3 (cons 4 nil)))

(append (cons 1 (cons 2 nil))
        (cons 3 (cons 4 nil)))

(def concat
	 (func (xs)
		   (if (isnil xs) nil
			 (append (head xs) (concat (tail xs))))))

(concat (cons l1 (cons l2 (cons l2 nil))))

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

(def list-bind
	 (func (xs f)
		   (concat (list-map f xs))))
list-bind

(def list-monad
	 (new monad
		  (pure list-pure)
		  (bind list-bind)))
list-monad

;; reader monad
(def reader-pure
	 (func (x)
		   (func (y) x)))
reader-pure


(def reader-bind
	 (func (a f)
		   (func (e)
				 ((f (a e)) e))))
reader-bind

(def reader-monad
	 (new monad
		  (pure reader-pure)
		  (bind reader-bind)))
reader-monad
