(import builtins)
(using builtins)

(let ((fact (fn (n)
				(if (= n 0) 1
                  (* n (fact (- n 1)))))))
  (fact 6))
