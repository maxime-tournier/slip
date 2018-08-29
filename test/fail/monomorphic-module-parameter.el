(def fst (fun (x y) x))

(import core)

(use core
	 (fun ((list x))
		  (fst (cons 1 x) (cons true x))))
