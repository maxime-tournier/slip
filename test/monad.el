

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

(def lmap
     (func (f x)
           (if (isnil x) nil
             (cons (f (head x)) (lmap f (tail x))))))

(def lpure
     (func (x) (cons x nil)))


(def lbind
	 (func (xs f)
		   (concat (lmap f xs))))
lbind

(new monad
	 (pure lpure)
	 (bind lbind))
