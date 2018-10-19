(def fst (fn (x y) x))

(import core)

(use core
     (fn ((list x))
         (fst (cons 1 x) (cons true x))))
