
(def (polymorphic-recursion x)
     (if true (polymorphic-recursion 1) (polymorphic-recursion false)))
