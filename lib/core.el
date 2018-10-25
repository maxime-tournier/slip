(import builtins)

(import list)

;; basic ops
(def + builtins.+)
(def - builtins.-)
(def * builtins.*)
(def = builtins.=)

;; types
(def type builtins.type)
(def ctor builtins.ctor)

(def -> builtins.->)

(def integer builtins.integer)
(def boolean builtins.boolean)
(def unit builtins.unit)

;; state
(def ref builtins.ref)
(def get builtins.get)
(def set builtins.set)

(def pure builtins.pure)
