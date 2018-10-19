
;; monoid module definition
(struct (monoid a)
        (empty a)
        (compose (a -> a -> a)))

;; monoid instance for integer
(def integer-monoid
     (new monoid
          (empty 0)
          (compose +)))

(def (concat (list lhs) rhs)
     (match lhs
            (nil _ rhs)
            (cons self (cons self.head (concat self.tail rhs)))))

;; monoid instance for list. note that we need to help the type checker by
;; annotating lhs in concat
(def list-monoid
     (new monoid
          (empty nil)
          (compose concat)))

