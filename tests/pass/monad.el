
(struct (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (>>= (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))


(def (maybe-bind (maybe a) f)
     (match a
            (none _ none)
            (some a (f a))))

(def maybe-monad
     (new monad
          (pure just)
          (>>= maybe-bind)))

maybe-monad.>>=

