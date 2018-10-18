

(def (concat lhs rhs)
     (match lhs
            (cons self (cons self.head (concat self.tail rhs)))
            (nil _ rhs)))

(def (range start end)
     (if (= start end) nil
       (cons start (range (+ start 1) end))))

(concat (range 0 10) (range 0 3))
