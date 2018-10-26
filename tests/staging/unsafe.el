(import core)
(use core)

(def x (ref list.nil))

(def (push x value)
     (do (bind y x)
         (bind z (get y))
         (set y (list.cons value z))))

(push x 1)
x

(push x true)
x


