(import core)
(using core)


(do
 (bind x (ref true))
 (do
  (pure (run (get x)))))
	   
