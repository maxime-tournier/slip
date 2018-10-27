(import core)
(using core)

(struct (functor (ctor f))
        (map (fn (a b) ((a -> b) -> (f a) -> (f b)))))

(def (map (functor f)) f.map)
     

                            
