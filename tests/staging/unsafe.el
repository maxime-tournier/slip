(import core)
(use core)

(def (push x value)
     (do (bind y x)
         (bind z (get y))
         (set y (list.cons value z))
		 (pure y)))

(push (ref list.nil) 1)

(push (ref list.nil) true)


