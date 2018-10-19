

(func ((integer x)) x)


(func ((pair x)) x)

(func ((functor f)) (:map f))


(func ((value x)) (:get x))


(let ((c (new value (get (func (x) 1)))))
  ((func ((value x)) (:get x)) c))


(let ((map (func (f x)
                 (if (isnil x) nil
                   (cons (f (head x)) (map f (tail x)))))))
  (new functor (map map)))

functor


(let ((coerce (func (x y) (if true x y))))
  (func (z)
        (new value (get (func (x) (coerce x z))))))
