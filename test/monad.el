

(def concat
     (func (lhs rhs)
           (if (isnil lhs) rhs
             (cons (head lhs)
                   (concat (tail lhs) rhs)))))

(concat (cons 1 (cons 2 nil))
        (cons 3 (cons 4 nil)))


(def lmap
     (func (f x)
           (if (isnil x) nil
             (cons (f (head x)) (lmap f (tail x))))))

(def lpure
     (func (x) (cons x nil)))

