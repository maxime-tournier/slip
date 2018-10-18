

;; basic arithmetic
(+ 1 2)
;; : integer = 3

;; empty list
nil
;; nil : list 'a = ()

;; list concatenation
(def (concat (list lhs) rhs)
	 (match lhs
			(nil _ rhs)
			(cons self (cons self.head
							 (concat self.tail rhs)))))

