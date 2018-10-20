(import core)
(use core)

;; build a list containing an integer range
(def (range start end)
     (if (= start end) list.nil
       (list.cons start (range (+ start 1) end))))

;; test list
(def test (list.concat (range 0 10) (range 0 3)))
test

;; doubling our test list
(list.map (fn (x) (* 2 x)) test)

(import func)


map

(map (fn (x) (* 2 x)) test)

