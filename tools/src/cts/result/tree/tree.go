package tree

import (
	"fmt"
	"io"
	"sort"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type QueryStatus struct {
	Query  query.Query
	Status result.Status
}

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
	if len(out) == 0 {
		return nil
	}
	return out
}

type Node struct {
	Query    query.Query
	Status   result.Status
	Children Nodes
}

func (n *Node) traverse(f func(n *Node) error) error {
	if err := f(n); err != nil {
		return err
	}
	for _, c := range n.Children {
		if err := c.traverse(f); err != nil {
			return err
		}
	}
	return nil
}

func (n *Node) list(l *result.List, tags result.Tags) {
	if n.Status != "" {
		*l = append(*l, result.Result{
			Query:  n.Query,
			Tags:   tags,
			Status: n.Status,
		})
	}
	for _, child := range n.Children {
		child.list(l, tags)
	}
}

const inconsistent mergedStatus = "<<inconsistent>>"

type mergedStatus result.Status

func (s *mergedStatus) add(n mergedStatus) {
	switch *s {
	case "":
		*s = mergedStatus(n)
	case mergedStatus(n):
		// Consistent status
	default:
		*s = inconsistent
	}
}

func (n *Node) merge() mergedStatus {
	targets := [query.TargetCount]mergedStatus{}
	status := mergedStatus(n.Status)

	for _, c := range n.Children {
		s := c.merge()
		target := c.Query.Target()
		targets[target].add(s)
		status.add(s)
	}

	n.Children = n.Children.filter(func(n *Node) bool {
		return targets[n.Query.Target()] == inconsistent
	})

	for i, s := range targets {
		if s != "" && s != inconsistent {
			t := query.Target(i)
			q := n.Query.Append(t, "*")
			n.getOrCreateChild(q).Status = result.Status(s)
		}
	}

	return status
}

func (n *Node) print(w io.Writer, prefix string) {
	fmt.Fprintf(w, "%v{\n", prefix)
	fmt.Fprintf(w, "%v  query:  '%v'\n", prefix, n.Query)
	fmt.Fprintf(w, "%v  status: '%v'\n", prefix, n.Status)
	for _, child := range n.Children {
		child.print(w, prefix+"  ")
	}
	fmt.Fprintf(w, "%v}\n", prefix)
}

func (n *Node) Format(f fmt.State, verb rune) {
	n.print(f, "")
}

func (n *Node) getOrCreateChild(query query.Query) *Node {
	if existing := n.Children.find(query); existing != nil {
		return existing
	}
	child := &Node{Query: query}
	n.Children.add(child)
	return child
}

type Tree struct {
	Node
}

// New constructs and returns a Tree from the list of result
func New(result result.List) (*Tree, error) {
	t := Tree{}
	for _, r := range result {
		if err := t.Add(r); err != nil {
			return nil, err
		}
	}
	return &t, nil
}

// Add adds a new result to the tree.
// Returns ErrDuplicateResult if the tree already contains a result for the
// given query.
func (t *Tree) Add(r result.Result) error {
	node := t.getOrCreateNode(r.Query)
	if node.Status != "" {
		return ErrDuplicateResult{node.Query}
	}
	node.Status = r.Status
	return nil
}

func (t *Tree) Reduce() {
	for _, root := range t.Node.Children {
		root.merge()
	}
}

func (t *Tree) ReduceTo(fq query.Query) error {
	node := &t.Node
	return fq.Walk(func(q query.Query, t query.Target, n string) error {
		if n == "*" {
			node.merge()
			return nil
		}
		node = node.Children.find(q)
		if node == nil {
			return ErrNoResultForQuery{q}
		}
		if q == fq {
			node.merge()
		}
		return nil
	})
}

func (t *Tree) AllResults(tags result.Tags) result.List {
	l := result.List{}
	t.list(&l, tags)
	return l
}

func (t *Tree) glob(fq query.Query, f func(f *Node) error) error {
	node := &t.Node

	return fq.Walk(func(q query.Query, t query.Target, n string) error {
		if n == "*" {
			for _, child := range node.Children {
				if child.Query.Target() == t {
					if err := child.traverse(f); err != nil {
						return err
					}
				}
			}
			return nil
		}
		switch t {
		case query.Suite, query.Files, query.Tests:
			child := node.Children.find(q)
			if child == nil {
				return ErrNoResultForQuery{q}
			}
			node = child
		case query.Cases:
			for _, child := range node.Children {
				if child.Query.Contains(q) {
					if err := f(child); err != nil {
						return err
					}
				}
			}
		}
		if q == fq {
			return f(node)
		}
		return nil
	})
}

func (t *Tree) getOrCreateNode(q query.Query) *Node {
	node := &t.Node
	q.Walk(func(q query.Query, t query.Target, n string) error {
		node = node.getOrCreateChild(q)
		return nil
	})
	return node
}

func (t *Tree) Replace(fq query.Query, s result.Status) error {
	node := &t.Node
	return fq.Walk(func(q query.Query, t query.Target, n string) error {
		if q == fq {
			node.Children = node.Children.filter(func(c *Node) bool {
				return !q.Contains(c.Query)
			})
			node = node.getOrCreateChild(q)
			node.Status = s
		} else {
			child := node.Children.find(q)
			if child == nil {
				return ErrNoResultForQuery{q}
			}
			node = child
		}
		return nil
	})
}

func (t *Tree) Collect(q query.Query) ([]QueryStatus, error) {
	out := []QueryStatus{}
	err := t.glob(q, func(n *Node) error {
		if n.Status != "" {
			out = append(out, QueryStatus{n.Query, n.Status})
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}
