(import core)
(use core)

(def (fib n)
     (if (= n 0) 0
       (if (= n 1) 1
         (+ (fib (- n 1))
            (fib (- n 2))))))

(fib 20)

