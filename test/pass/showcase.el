

;; simple arithmetic
(+ 1 2)

;; empty list
nil

;; list concatenation, pattern matching
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
(def (list-map f (list x))
     (match x
            (nil _ nil)
            (cons self (cons (f self.head) (list-map f self.tail)))))

;; doubling our test list
(list-map (fn (x) (* 2 x)) test)


;; functor module definition
(module (functor (ctor f))
        (map (fn (a b) ((a -> b) -> (f a) -> (f b)))))

;; list functor instance
(new functor
     (map list-map))

;; maybe functor instance
(new functor
     (map (fn (f (maybe a))
              (match a
                     (none _ none)
                     (some a (just (f a)))))))

;; omg monads!!11
(module (monad (ctor m))
        (pure (fn (a) (a -> (m a))))
        (>>= (fn (a b) ((m a) -> (a -> (m b)) -> (m b)))))

;; maybe monad instance
(new monad
     (pure just)
     (>>= (fn ((maybe a) f)
              (match a
                     (none _ none)
                     (some a (f a))))))

(def (maybe-map f (monad a))
     (match a
            (none _ none)
            (some self (f self))))

;; maybe functor instance
(new functor (map maybe-map))
     
