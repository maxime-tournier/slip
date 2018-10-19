
(union (maybe a)
       (none unit)
       (some a))

(def none (new maybe (none ())))
(def (just a) (new maybe (some a)))

