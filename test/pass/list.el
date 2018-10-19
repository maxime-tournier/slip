
;; list concatenation
(def (concat lhs rhs)
     (match lhs
            (cons self (cons self.head (concat self.tail rhs)))
            (nil _ rhs)))

;; build a list containing an integer range
(def (range start end)
     (if (= start end) nil
       (cons start (range (+ start 1) end))))


;; test list
(def test (concat (range 0 10) (range 0 3)))
test

;; list map
(def (map f x)
     (match x
            (nil _ nil)
            (cons self (cons (f self.head) (map f self.tail)))))

;; doubling our test list
(map (fn (x) (* 2 x)) test)
