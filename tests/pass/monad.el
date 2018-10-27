(import core)
(using core)

(struct (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (>>= (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))


