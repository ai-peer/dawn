package tree

import (
	"errors"
	"fmt"
	"io"
	"sort"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type Nodes []*Node

func (l Nodes) find(q query.Query) *Node {
	for _, child := range l {
		if child.Query == q {
			return child
		}
	}
	return nil
}

// TODO: binary search
func (l *Nodes) add(n *Node) {
	*l = append(*l, n)
	sort.Slice(*l, func(i, j int) bool {
		return (*l)[i].Query.Compare((*l)[j].Query) < 0
	})
}

// TODO: binary search
func (l *Nodes) remove(node *Node) {
	out := make(Nodes, 0, len(*l))
	for _, n := range *l {
		if n != node {
			out = append(out, n)
		}
	}
	*l = out
}

func (l Nodes) filter(f func(*Node) bool) Nodes {
	out := make(Nodes, 0, len(l))
	for _, n := range l {
		if f(n) {
			out = append(out, n)
		}
	}
	return out
}

type Node struct {
	Query    query.Query
	Results  map[result.Variant]result.Status
	Children Nodes
}

func (n *Node) list(l *result.List) {
	for _, v := range n.sortedResultVariants() {
		*l = append(*l, result.Result{
			Query:   n.Query,
			Status:  n.Results[v],
			Variant: v,
		})
	}
	for _, child := range n.Children {
		child.list(l)
	}
}

func (n *Node) promoteCommonStatus() {
	for _, child := range n.Children {
		child.promoteCommonStatus()
	}

	const Inconsistent result.Status = "<<inconsistent>>"
	results := map[result.Variant]result.Status{}
	for _, child := range n.Children {
		for variant, status := range child.Results {
			if existing, ok := results[variant]; ok && existing != status {
				results[variant] = Inconsistent
			} else {
				results[variant] = status
			}
		}
	}

	for variant, status := range results {
		if status != Inconsistent {
			n.addResult(variant, status)
		}
	}
}

func (n *Node) pruneCommonStatus() error {
	if n.Children != nil && n.Results != nil {
		// Non-leaf node with common results between children
		for variant, status := range n.Results {
			for _, child := range n.Children {
				if _, ok := child.Results[variant]; ok {
					target := child.Query.Target()
					query := n.Query.Append(target, "*")
					wildcard, err := n.getOrCreateChild(query)
					if err != nil {
						return err
					}
					if child != wildcard {
						wildcard.addResult(variant, status)
						child.removeResult(variant)
					}
				}
			}
		}

		n.Results = nil

		n.Children = n.Children.filter(func(child *Node) bool {
			return child.Results != nil
		})
	}

	for _, child := range n.Children {
		if err := child.pruneCommonStatus(); err != nil {
			return err
		}
	}

	return nil
}

func (n *Node) reduce() {
	n.promoteCommonStatus()
	n.pruneCommonStatus()
}

func (n *Node) sortedResultVariants() []result.Variant {
	out := make([]result.Variant, 0, len(n.Results))
	for v := range n.Results {
		out = append(out, v)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Compare(out[j]) < 0 })
	return out
}

func (n *Node) print(w io.Writer, prefix string) {
	fmt.Fprintf(w, "%v{\n", prefix)
	fmt.Fprintf(w, "%v  query:  '%v'\n", prefix, n.Query)
	for v, s := range n.Results {
		fmt.Fprintf(w, "%v  %v: %v\n", prefix, v, s)
	}
	for _, child := range n.Children {
		child.print(w, prefix+"  ")
	}
	fmt.Fprintf(w, "%v}\n", prefix)
}

func (n *Node) Format(f fmt.State, verb rune) {
	n.print(f, "")
}

func (n *Node) removeResult(variant result.Variant) {
	delete(n.Results, variant)
	if len(n.Results) == 0 {
		n.Results = nil
	}
}

func (n *Node) addResult(v result.Variant, s result.Status) bool {
	if n.Results == nil {
		n.Results = map[result.Variant]result.Status{}
	}
	if existing, ok := n.Results[v]; ok && existing != s {
		return false
	}
	n.Results[v] = s
	return true
}

func (n *Node) gatherResults(v result.Variant, out *result.List) {
	if status, ok := n.Results[v]; ok {
		*out = append(*out, result.Result{
			Query: n.Query, Variant: v, Status: status,
		})
	} else {
		for _, child := range n.Children {
			child.gatherResults(v, out)
		}
	}
}

func (n *Node) getOrCreateChild(query query.Query) (*Node, error) {
	if existing := n.Children.find(query); existing != nil {
		return existing, nil
	}
	child := &Node{Query: query}
	n.Children.add(child)
	return child, nil
}

type Tree struct {
	Node
}

// New constructs and returns a Tree from the list of results
func New(results result.List) (*Tree, error) {
	t := Tree{}
	for _, r := range results {
		node, err := t.node(r.Query)
		if err != nil {
			return nil, err
		}
		if !node.addResult(r.Variant, r.Status) {
			return nil, fmt.Errorf("'%v' has duplicate results for variant '%v'", node.Query, r.Variant)
		}
	}
	return &t, nil
}

func (t *Tree) Reduce() {
	for _, root := range t.Node.Children {
		root.reduce()
	}
}

func (t *Tree) AllResults() result.List {
	l := result.List{}
	t.list(&l)
	return l
}

func (t *Tree) Results(q query.Query, v result.Variant) result.List {
	node := &t.Node
	q.Walk(func(c query.Query) error {
		child := node.Children.find(c)
		if child == nil {
			return errors.New("stop")
		}
		node = child
		return nil
	})

	if !q.Contains(node.Query) {
		return nil
	}

	out := result.List{}
	node.gatherResults(v, &out)
	return out
}

func (t *Tree) node(q query.Query) (*Node, error) {
	node := &t.Node
	err := q.Walk(func(q query.Query) error {
		var err error
		node, err = node.getOrCreateChild(q)
		return err
	})
	if err != nil {
		return nil, err
	}
	return node, nil
}
