
(module (functor (ctor f))
        (map (fn (a b) ((a -> b) -> (f a) -> (f b)))))

(def list-map
     (fn (f x)
         (if (isnil x) nil
           (cons (f (head x)) (list-map f (tail x))))))

(def list-functor
     (new functor
          (map list-map)))


                            
