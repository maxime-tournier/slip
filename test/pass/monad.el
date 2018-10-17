
(module (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (bind (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))

(def pure (fn ((monad m)) m.pure))
(def join (fn ((monad m)) m.bind))
