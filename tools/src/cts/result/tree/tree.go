package tree

import (
	"fmt"
	"io"
	"sort"
	"strings"

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

const inconsistent result.Status = "<<inconsistent>>"

type targetVariant struct {
	target  query.Target
	variant result.Variant
}
type commonNodeResults map[*Node]map[targetVariant]result.Status

func (r commonNodeResults) collect(n *Node) {
	add := func(tv targetVariant, status result.Status) {
		m, ok := r[n]
		if !ok {
			m = map[targetVariant]result.Status{}
			r[n] = m
		}

		if existing, ok := m[tv]; ok && existing != status {
			m[tv] = inconsistent
		} else {
			m[tv] = status
		}
	}

	if n.Children == nil {
		// Leaf
		for v, s := range n.Results {
			add(targetVariant{n.Query.Target(), v}, s)
		}
	} else {
		// Non-leaf
		for _, c := range n.Children {
			r.collect(c)
			for tv, s := range r[c] {
				add(targetVariant{c.Query.Target(), tv.variant}, s)
			}
		}

		// Remove consistent results from children
		for ntv, s := range r[n] {
			if s != inconsistent {
				for _, c := range n.Children {
					for ctv := range r[c] {
						if ctv.variant == ntv.variant {
							delete(r[c], ctv)
						}
					}
				}
			}
		}
	}
}

func (n *Node) prune(common commonNodeResults) error {
	for _, c := range n.Children {
		if err := c.prune(common); err != nil {
			return err
		}
	}

	n.Results = nil

	for tv, s := range common[n] {
		if s == inconsistent {
			continue
		}
		if n.Children != nil {
			wildcard := n.getOrCreateChild(n.Query.Append(tv.target, "*"))
			wildcard.addResult(tv.variant, s)
		} else {
			n.addResult(tv.variant, s)
		}
	}

	n.Children = n.Children.filter(func(c *Node) bool {
		return c.Results != nil || c.Children != nil
	})

	return nil
}

// DISABLED: Not convinced this is sound
const EnableSimplifyCases = false

func (n *Node) simplifyCases() error {
	if !EnableSimplifyCases {
		return nil
	}

	caseChildren := []*Node{}

	for _, child := range n.Children {
		if err := child.simplifyCases(); err != nil {
			return err
		}
		if child.Query.Target() == query.Cases {
			caseChildren = append(caseChildren, child)
		}
	}

	if len(caseChildren) == 0 {
		return nil
	}

	variants := result.VariantSet{}
	for _, c := range caseChildren {
		for v := range c.Results {
			variants.Add(v)
		}
	}

	for v := range variants {
		keys := map[string]result.Status{}
		// Look at all the 'key=value' case parameters.
		// Build a map of key to common status.
		for _, c := range caseChildren {
			status := c.Results[v]
			for key := range c.Query.SplitCases() {
				if s, ok := keys[key]; ok && s != status {
					keys[key] = inconsistent
				} else {
					keys[key] = status
				}
			}
		}
		// Remove all inconsistent keys.
		for key, status := range keys {
			if status == inconsistent {
				delete(keys, key)
			}
		}
		if len(keys) > 0 {
			for _, c := range caseChildren {
				reduced := []string{}
				for _, param := range strings.Split(c.Query.Cases, query.CaseDelimiter) {
					if parts := strings.Split(param, "="); len(parts) == 2 {
						if _, omit := keys[parts[0]]; omit {
							continue
						}
					}
					reduced = append(reduced, param)
				}

				q := c.Query
				q.Cases = strings.Join(reduced, query.CaseDelimiter)

				r := c.Results[v]
				c.removeResult(v)
				n.getOrCreateChild(q).addResult(v, r)
			}
		}
	}

	return nil
}

func (n *Node) reduce() {
	r := commonNodeResults{}
	r.collect(n)
	n.prune(r)
	n.simplifyCases()
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

// New constructs and returns a Tree from the list of results
func New(results result.List) (*Tree, error) {
	t := Tree{}
	for _, r := range results {
		node, err := t.getOrCreateNode(r.Query)
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

type ErrNoResultsForQuery struct {
	Query query.Query
}

func (e ErrNoResultsForQuery) Error() string {
	return fmt.Sprintf("no results for query '%v'", e.Query)
}

func (t *Tree) glob(q query.Query, f func(f *Node) error) error {
	node := &t.Node

	return q.Walk(func(q query.Query, t query.Target, n string) error {
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
				return ErrNoResultsForQuery{q}
			}
			node = child
		case query.Cases:
			for _, child := range node.Children {
				if q.Contains(child.Query) {
					if err := f(child); err != nil {
						return err
					}
				}
			}
		}
		return nil
	})
}

func (t *Tree) getOrCreateNode(q query.Query) (*Node, error) {
	node := &t.Node
	err := q.Walk(func(q query.Query, t query.Target, n string) error {
		node = node.getOrCreateChild(q)
		return nil
	})
	if err != nil {
		return nil, err
	}
	return node, nil
}

func (t *Tree) Replace(query query.Query, variant result.Variant, status result.Status) error {
	err := t.glob(query, func(n *Node) error {
		n.removeResult(variant)
		return nil
	})
	if err != nil {
		return err
	}
	node, err := t.getOrCreateNode(query)
	if err != nil {
		return err
	}
	node.addResult(variant, status)
	return nil
}

func (t *Tree) Results(query query.Query, variant result.Variant) (result.List, error) {
	out := result.List{}
	err := t.glob(query, func(n *Node) error {
		if status, ok := n.Results[variant]; ok {
			out = append(out, result.Result{
				Query: n.Query, Variant: variant, Status: status,
			})
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}
