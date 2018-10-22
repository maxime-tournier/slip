(import builtin)

;; recursive tree/node types
(def type
     (let
         ((tree (union (tree a)
                       (leaf a)
                       (node (node a))))
          (node (struct (node a)
                        (value a)
                        (left (tree a))
                        (right (tree a)))))
       (record (tree tree)
               (node node))))

(def tree type.tree)
tree

(def node type.node)
node

;; data constructors
(def (make-leaf a)
     (new tree (leaf a)))
make-leaf

;; 
(def (make-node a lhs rhs)
     (new tree
          (node (new node
                     (value a)
                     (left lhs)
                     (right rhs)))))
make-node

(make-leaf 2)

(def test (make-node 1 (make-leaf 2) (make-leaf 3)))
test

(import list)
(def (dfs (tree self) acc)
     (match self
            (leaf a (list.cons a acc))
            (node (node n) (dfs n.left (list.cons n.value (dfs n.right acc))))))
dfs

(dfs test list.nil)
