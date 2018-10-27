(import core)
(using core.list)

(def fst (fn (x y) x))

(fn ((list x))
    (fst (cons 1 x) (cons true x)))
