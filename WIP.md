
(struct (foo ...)
		(struct (bar...) ...)

		(id (a -> a)))


(struct (functor (ctor f))

		(map (fn (a b)
				 (a -> b -> (f a) -> (f b)))))


(def functor
	 (fn ((ctor f))
		 (module
		  (map (fn (a b) (a -> b -> (f a) -> (f b))))
		  )))


functor :: (ctor f) -> (type (functor f))


(def functor
	 (fn ((ctor f))
		  (record
		   (map (fn (a b) (a -> b -> (f a) -> (f b)))))))

functor :: (ctor f) -> {map: (type a) -> (type b) -> (type (a -> b -> (f a) -> (f b)))}

est-ce que ca définit un type (functor f) ?


exactement sherlock

signatures:

integer :: type integer
list :: (type a) -> (type list a)
functor :: (ctor f) -> (type {map: ...})

args... -> inner

lift: type 'a -> a

TODO associer (functor f) à un type unique et c'est bon

- pour ouvrir/fermer un module, il faut sa signature dans le scope
- un module fermé peut ne plus être ouvert s'il est renvoyé par une fonction
- modules generatifs (TODO est-ce qu'on peut/veut des foncteurs applicatifs?)


		
		
# en gros

2 options pour réifier les types: 

1. `type` monomorphe. 
2. `type a` polymorphe

## 1 

-> :: type -> type -> type

+ facile/trivial d'inférer les kinds et les types des modules/attributs

1.1 soit on arrive d'une manière unique à associer les constructeurs réifiés à leur
constructeur (via leur type? nom?) mais ca parait compliqué


1.2 soit chaque nom de type est associé à un constructeur réifié et une signature dans
l'environnement de type (hierarchique, qui suit l'environnement des valeurs)

  - ca implique que les types reifiés **doivent** etre des variables et ne
    peuvent pas provenir d'expressions comme core.list
	
  - on ne peut ouvrir/désigner que des types qui sont dans le scope

  - pour mettre des types dans le scope il faut importer un module avec des
    types nestés (ou un package, selon)
  
  - du coup il faut que les modules exposent les types nestés d'une manière ou
    d'un autre et c'est la merde
  
  
## 2 
  
  -> :: (type 'a) -> (type 'b) -> (type (a -> b))
  
  
  + il est facile d'associer un type reifié à son type source, puisqu'il
  apparait dans le type du type reifie. du coup on peut facilement avoir une
  table de signature globale où aller chercher la signature pour un type à
  partir d'un type réifié
  
  
  + du coup on peut facilement avoir des types membres core.list à partir du
    moment où les signatures ont été enregistrées correctement
  
  - ca complique l'utilisation des constructueurs qui doivent etre utilises de
    maniere polymorphe dans les definitions de types:
	
	(fn ((ctor f) a b) (a -> b -> (f a) -> (f b)))
	
	f est utilisé avec deux types différents donc doit etre ouvert comme un
    module
	
  - du coup ca complique aussi la définition des modules polymorphes puisque la
    definition doit elle-meme ouvrir les constructeurs en arguments
	
  + en meme temps on peut facilement reutiliser le code pour `abs` et définir le
    foncteur uniquement à partir de la signature paramétrée:
	
	```
	(module test
		(fn ((ctor f))
			(reify 
				(record 
				(pure (fn (a) (a -> (f a))))))))
				
	```

    la signature a le type:
	
	(ctor f) -> {pure: (type a) -> (type (a -> (f a)))}

    - on peut facilement sortir le type de `pure` via la meme operation qui
      reconstruit un type à partir d'un constructeur réifié
	
	- idem pour le type d'ouverture du module
	
	- il reste à creer un type `test` avec son kind
	
	- donc il faut trouver le kind à partir du type de la signature
	
	- on définit le constructeur reifié:
	test :: (ctor f) -> (type (test f))
	
	- on finit le type d'ouverture: test ~ (test f) -> {pure: a -> (f a)}
	
	et on est bons.
	
     
	
## 3 combiner les 2 ? 

- à la 1ml: `(type 'a)` est plus ou moins un sous-type (existentiel) de `type`,
  de sorte qu'on peut utiliser `(type a)` là où on attend un `type`
  
- la version 1 permet pratiquement de reconstruire la version `(type a)` à
  partir des définitions des modules: 
  
  (module (foo f)
	  (pure (fn (a) (a -> (f a)))))
	  
  il faudrait pouvoir "ouvrir" -> en son vrai type mais ca n'est pas possible
  
- par contre à partir du vrai type de -> on peut facilement synthétiser une
  version à la 2, typechecker la définition du module et des attributs avec,
  puis reconvertir?
  
  en partant de -> :: (type a) -> (type b) -> (type (a -> b))
  on obtient : type -> type -> type
  
  on obtient: a :: type, f :: type -> type
  pure : type -> type (~ forall a. (a -> a))
  
  ensuite on a les kinds de tout le monde, donc on peut refaire une passe:
  
  f ~ forall a. (type a) -> (type (!f a))
  a ~ (type !a)
  
  

  

  
  
  

