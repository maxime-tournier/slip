
(module (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (mbind (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))

(def pure (fn ((monad m)) m.pure))
(def mbind (fn ((monad m)) m.mbind))
