;; list module
(import builtins)
(import func)

(def cons builtins.cons)
(def nil builtins.nil)
(def list builtins.list)

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

;; TODO use builtins + export size
;; list size
(def (size (list self))
     (match self
            (nil _ 0)
            (cons self (builtins.+ 1 (size self.tail)))))


;; reverse a list
(def reverse (foldl (func.flip cons) nil))


;; functor map
(def (map f)
     (foldr (fn (x xs)
                (cons (f x) xs)) nil))



