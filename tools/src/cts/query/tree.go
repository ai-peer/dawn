// Copyright 2022 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package query

import (
	"fmt"
	"io"
	"sort"
)

// Tree holds a tree structure of Query to generic Data type.
// Each separate suite, file, test of the query produces a separate tree node.
// All cases of the query produce a single leaf tree node.
type Tree[Data any] struct {
	TreeNode[Data]
}

type TreeNode[Data any] struct {
	Query    Query
	Data     *Data
	Children TreeNodeChildren[Data]
}

type TreeNodeChildKey struct {
	Name   string
	Target Target
}

type TreeNodeChildren[Data any] map[TreeNodeChildKey]*TreeNode[Data]

func (n *TreeNode[Data]) sortedChildKeys() []TreeNodeChildKey {
	keys := make([]TreeNodeChildKey, 0, len(n.Children))
	for key := range n.Children {
		keys = append(keys, key)
	}
	sort.Slice(keys, func(i, j int) bool {
		a, b := keys[i], keys[j]
		switch {
		case a.Name < b.Name:
			return true
		case a.Name > b.Name:
			return false
		case a.Target < b.Target:
			return true
		case a.Target > b.Target:
			return false
		}
		return false
	})
	return keys
}

func (n *TreeNode[Data]) traverse(f func(n *TreeNode[Data]) error) error {
	if err := f(n); err != nil {
		return err
	}
	for _, key := range n.sortedChildKeys() {
		if err := n.Children[key].traverse(f); err != nil {
			return err
		}
	}
	return nil
}

type Merger[Data any] func([]Data) *Data

// merge collapses tree nodes based on child node data, using the function f.
// Returns the merged target data for this node.
func (n *TreeNode[Data]) merge(f Merger[Data]) *Data {
	// If the node is a leaf, then simply return the node's data.
	if len(n.Children) == 0 {
		return n.Data
	}

	// Build a map of child target to merged child data.
	// A nil for the value indicates that one or more children could not merge.
	mergedChildren := map[Target][]Data{}
	for key, child := range n.Children {
		// Call merge() on the child. Even if we cannot merge this node, we want
		// to do this for all children so they can merge their sub-graphs.
		childData := child.merge(f)

		if childData == nil {
			// If merge() returned nil, then the data could not be merged.
			// Mark the entire target as unmergeable.
			mergedChildren[key.Target] = nil
			continue
		}

		// Fetch the merge list for this child's target.
		list, found := mergedChildren[key.Target]
		if !found {
			// First child with the given target?
			mergedChildren[key.Target] = []Data{*childData}
			continue
		}
		if list != nil {
			mergedChildren[key.Target] = append(list, *childData)
		}
	}

	merge := func(in []Data) *Data {
		switch len(in) {
		case 0:
			return nil // nothing to merge.
		case 1:
			return &in[0] // merge of a single item results in that item
		default:
			return f(in)
		}
	}

	// Might it possible to merge this node?
	maybeMergeable := true

	// The merged data, per target
	mergedTargets := map[Target]Data{}

	// Attempt to merge each of the target's data
	for target, list := range mergedChildren {
		if list != nil { // nil == unmergeable target
			if data := merge(list); data != nil {
				// Merge success!
				mergedTargets[target] = *data
				continue
			}
		}
		maybeMergeable = false // Merge of this node is not possible
	}

	// Remove all children that have been merged
	for key := range n.Children {
		if _, merged := mergedTargets[key.Target]; merged {
			delete(n.Children, key)
		}
	}

	// Add wildcards for merged targets
	for target, data := range mergedTargets {
		t := Target(target)
		data := data // Don't take address of iterator
		n.getOrCreateChild(TreeNodeChildKey{"*", t}).Data = &data
	}

	// If any of the targets are unmergeable, then we cannot merge the node itself.
	if !maybeMergeable {
		return nil
	}

	// All targets were merged. Attempt to merge each of the targets.
	data := make([]Data, 0, len(mergedTargets))
	for _, d := range mergedTargets {
		data = append(data, d)
	}
	return merge(data)
}

func (n *TreeNode[Data]) print(w io.Writer, prefix string) {
	fmt.Fprintf(w, "%v{\n", prefix)
	fmt.Fprintf(w, "%v  query: '%v'\n", prefix, n.Query)
	fmt.Fprintf(w, "%v  data:  '%v'\n", prefix, n.Data)
	for _, key := range n.sortedChildKeys() {
		n.Children[key].print(w, prefix+"  ")
	}
	fmt.Fprintf(w, "%v}\n", prefix)
}

func (n *TreeNode[Data]) Format(f fmt.State, verb rune) {
	n.print(f, "")
}

func (n *TreeNode[Data]) getOrCreateChild(key TreeNodeChildKey) *TreeNode[Data] {
	if n.Children == nil {
		child := &TreeNode[Data]{Query: n.Query.Append(key.Target, key.Name)}
		n.Children = TreeNodeChildren[Data]{key: child}
		return child
	}
	if child, ok := n.Children[key]; ok {
		return child
	}
	child := &TreeNode[Data]{Query: n.Query.Append(key.Target, key.Name)}
	n.Children[key] = child
	return child
}

// QueryData is a pair of a Query and a generic Data type.
// Used by NewTree for constructing a tree with entries.
type QueryData[Data any] struct {
	Query Query
	Data  Data
}

func NewTree[Data any](entries ...QueryData[Data]) (Tree[Data], error) {
	out := Tree[Data]{}
	for _, qd := range entries {
		if err := out.Add(qd.Query, qd.Data); err != nil {
			return Tree[Data]{}, err
		}
	}
	return out, nil
}

// Add adds a new data to the tree.
// Returns ErrDuplicateData if the tree already contains a data for the given
func (t *Tree[Data]) Add(q Query, d Data) error {
	node := t.getOrCreateNode(q)
	if node.Data != nil {
		return ErrDuplicateData{node.Query}
	}
	node.Data = &d
	return nil
}

func (t *Tree[Data]) Reduce(f Merger[Data]) {
	for _, root := range t.TreeNode.Children {
		root.merge(f)
	}
}

func (t *Tree[Data]) ReduceTo(to Query, f Merger[Data]) error {
	node := &t.TreeNode
	return to.Walk(func(q Query, t Target, n string) error {
		if n == "*" {
			node.merge(f)
			return nil
		}
		child, ok := node.Children[TreeNodeChildKey{n, t}]
		if !ok {
			return ErrNoDataForQuery{q}
		}
		node = child
		if q == to {
			node.merge(f)
		}
		return nil
	})
}

func (t *Tree[Data]) glob(fq Query, f func(f *TreeNode[Data]) error) error {
	node := &t.TreeNode
	return fq.Walk(func(q Query, t Target, n string) error {
		if n == "*" {
			for _, key := range node.sortedChildKeys() {
				child := node.Children[key]
				if child.Query.Target() == t {
					if err := child.traverse(f); err != nil {
						return err
					}
				}
			}
			return nil
		}
		switch t {
		case Suite, Files, Tests:
			child, ok := node.Children[TreeNodeChildKey{n, t}]
			if !ok {
				return ErrNoDataForQuery{q}
			}
			node = child
		case Cases:
			for _, key := range node.sortedChildKeys() {
				child := node.Children[key]
				if child.Query.Contains(fq) {
					if err := f(child); err != nil {
						return err
					}
				}
			}
			return nil
		}
		if q == fq {
			return node.traverse(f)
		}
		return nil
	})
}

func (t *Tree[Data]) getOrCreateNode(q Query) *TreeNode[Data] {
	node := &t.TreeNode
	q.Walk(func(q Query, t Target, n string) error {
		node = node.getOrCreateChild(TreeNodeChildKey{n, t})
		return nil
	})
	return node
}

func (t *Tree[Data]) Replace(what Query, with Data) error {
	node := &t.TreeNode
	return what.Walk(func(q Query, t Target, n string) error {
		childKey := TreeNodeChildKey{n, t}
		if q == what {
			for key, child := range node.Children {
				if q.Contains(child.Query) {
					delete(node.Children, key)
				}
			}
			node = node.getOrCreateChild(childKey)
			node.Data = &with
		} else {
			child, ok := node.Children[childKey]
			if !ok {
				return ErrNoDataForQuery{q}
			}
			node = child
		}
		return nil
	})
}

func (t *Tree[Data]) List() []QueryData[Data] {
	out := []QueryData[Data]{}
	t.traverse(func(n *TreeNode[Data]) error {
		if n.Data != nil {
			out = append(out, QueryData[Data]{n.Query, *n.Data})
		}
		return nil
	})
	return out
}

func (t *Tree[Data]) Glob(q Query) ([]QueryData[Data], error) {
	out := []QueryData[Data]{}
	err := t.glob(q, func(n *TreeNode[Data]) error {
		if n.Data != nil {
			out = append(out, QueryData[Data]{n.Query, *n.Data})
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}
