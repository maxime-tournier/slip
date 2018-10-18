

(def (concat lhs rhs)
     (match lhs
            (cons self (cons self.head (concat self.tail rhs)))
            (nil _ rhs)))

(def (range start end)
     (if (= start end) nil
       (cons start (range (+ start 1) end))))

(def test (concat (range 0 10) (range 0 3)))

(def (map f x)
     (match x
            (nil _ nil)
            (cons self (cons (f self.head) (map f self.tail)))))

(map (fn (x) (* 2 x)) test)
