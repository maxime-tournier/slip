(def map (func ((functor f)) (:map f)))

(def lmap (func (f x)
                (if (isnil x) nil
                  (cons (f (head x)) (lmap f (tail x))))))

(def lfunctor (new functor (map lmap)))


(def test (cons 1 (cons 2 (cons 3 nil))))
;; (lmap (func (x) (+ 1 x)) test)


                  
