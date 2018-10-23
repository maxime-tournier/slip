(import builtins)

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

;; leaf constructors
(def (make-leaf a)
     (new tree (leaf a)))
make-leaf

;; node constructor
(def (make-node a lhs rhs)
     (new tree
          (node (new node
                     (value a)
                     (left lhs)
                     (right rhs)))))
make-node

(make-leaf 2)

(def test1 (make-node 1 (make-leaf 2) (make-leaf 3)))
(def test (make-node 10 test1 test1))
;; depth-first search
(import list)
(def (dfs (tree self) acc)
     (match self
            (leaf a (list.cons a acc))
            (node (node n)
                  (dfs n.left
                       (list.cons n.value
                                  (dfs n.right acc))))))
dfs

(dfs test list.nil)
