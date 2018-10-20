(import core)
(use core)

;; monoid module definition
(struct (monoid a)
        (empty a)
        (compose (a -> a -> a)))

;; monoid instance for integer
(def integer-monoid
     (new monoid
          (empty 0)
          (compose +)))

;; monoid instance for list. note that we need to help the type checker by
;; annotating lhs in concat
(def list-monoid
     (new monoid
          (empty list.nil)
          (compose list.concat)))

