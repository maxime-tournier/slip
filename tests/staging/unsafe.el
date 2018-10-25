(import core)
(use core)

(def x (ref list.nil))

(do (bind y x)
	(pure y))
