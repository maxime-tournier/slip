
;; monoid module definition
(struct (monoid a)
        (empty a)
        (compose (a -> a -> a)))

;; monoid instance for integer
(def integer-monoid
     (new monoid
          (empty 0)
          (compose +)))

(def (list-concat lhs rhs)
     (if (isnil lhs) rhs
       (cons (head lhs) (list-concat (tail lhs) rhs))))

;; monoid instance for list
(def list-monoid
     (new monoid
          (empty nil)
          (compose list-concat)))

