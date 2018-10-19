;; (import builtin)

(def list builtin.list)
(def cons builtin.cons)
(def nil builtin.nil)

(def (concat lhs (list rhs))
     (match lhs
            (nil _ rhs)
            (cons self (cons self.head
                             (concat self.tail rhs)))))
