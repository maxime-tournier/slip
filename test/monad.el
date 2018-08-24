

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

(def list-map
     (func (f x)
           (if (isnil x) nil
             (cons (f (head x)) (list-map f (tail x))))))

(def list-pure
     (func (x) (cons x nil)))


(def list-bind
	 (func (xs f)
		   (concat (list-map f xs))))
list-bind

(def list-monad
	 (new monad
		  (pure list-pure)
		  (bind list-bind)))
list-monad

