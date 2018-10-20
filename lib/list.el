;; list module
(import builtin)

(def list builtin.list)
(def cons builtin.cons)
(def nil builtin.nil)

;; list concatenation
(def (concat (list lhs) (list rhs))
     (match lhs
            (nil _ rhs)
            (cons self (cons self.head
                             (concat self.tail rhs)))))

;; TODO use builtin + export size
;; list size
(def (size (list self))
	 (match self
			(nil _ 0)
			(cons self (builtin.+ 1 (size self.tail)))))


;; reverse a list
(def (reverse (list self))
	 (let ((helper
			(fn ((list self) acc)
				(match self
					   (nil _ acc)
					   (cons self (helper
								   self.tail
								   (cons self.head acc)))))))
	   (helper self nil)))

;; functor map
(def (map f (list self))
	 (match self
			(nil _ nil)
			(cons self (cons (f self.head)
							 (map f self.tail)))))
