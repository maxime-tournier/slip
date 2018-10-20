;; list module
(import builtin)
(import func)

(def list builtin.list)
(def cons builtin.cons)
(def nil builtin.nil)


;; folds
(def (foldr f init (list self))
	 (match self
			(nil _ init)
			(cons self
				  (f self.head (foldr f init self.tail)))))

(def (foldl f init (list self))
	 (match self
			(nil _ init)
			(cons self
				  (foldl f (f init self.head) self.tail))))

;; list concatenation
(def (concat (list lhs) (list rhs))
	 (foldr cons rhs lhs))

;; TODO use builtin + export size
;; list size
(def (size (list self))
	 (match self
			(nil _ 0)
			(cons self (builtin.+ 1 (size self.tail)))))


;; reverse a list
(def reverse (foldl (func.flip cons) nil))

;; functor map
(def (map f (list self))
	 (match self
			(nil _ nil)
			(cons self (cons (f self.head)
							 (map f self.tail)))))



