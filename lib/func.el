
;; reverse arguments
(def (flip f x y) (f y x))

;; function application
(def (apply f x) (f x))

;; composition
(def (compose f g)
	 (fn (x) (f (g x))))

