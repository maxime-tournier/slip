(import core)
(using core)

(let ((fact (fn (n)
                (if (= n 0) 1
                  (* n (fact (- n 1)))))))
  (fact 5))


