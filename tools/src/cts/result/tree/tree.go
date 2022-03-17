package tree

import (
	"fmt"
	"io"
	"regexp"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

// Level is an enumerator of test node 'levels'.
type Level int

// suite:*
// suite:file,*
// suite:file,file,*
// suite:file,file,file:test:*
// suite:file,file,file:test,test:case;*
const (
	Suite Level = 1
	File  Level = 2
	Test  Level = 3
	Case  Level = 4
)

// Format writes the Level to the fmt.State
func (l Level) Format(f fmt.State, verb rune) {
	switch l {
	case Suite:
		fmt.Fprint(f, "suite")
	case File:
		fmt.Fprint(f, "file")
	case Test:
		fmt.Fprint(f, "test")
	case Case:
		fmt.Fprint(f, "case")
	default:
		fmt.Fprint(f, "<invalid>")
	}
}

func delimiter(parent, child Level) rune {
	if parent == child {
		return ','
	}
	return ':'
}

type Node struct {
	Name     string
	Level    Level
	Results  map[result.Variant]result.Status
	Children map[string]*Node
}

func (n *Node) promoteCommonStatus(variant result.Variant) {
	var status result.Status
	for _, child := range n.Children {
		s, ok := child.Results[variant]
		if !ok {
			return // Child does not have a result for this status.
		}
		if status == "" {
			status = s
		} else if s != status {
			return // Statuses are inconsistent
		}
	}
	// All children have the same status. Hoist this up to this node.
	n.addResult(variant, status)
	for _, child := range n.Children {
		child.removeResult(variant)
	}
}

func (n *Node) list(l *result.List) {
	for _, v := range n.sortedResultVariants() {
		wildcard := ":*"
		switch n.Level {
		case File:
			wildcard = ",*"
		case Test:
			wildcard = ":*"
		case Case:
			wildcard = ";*"
		}
		*l = append(*l, result.Result{
			Name:    n.Name + wildcard,
			Status:  n.Results[v],
			Variant: v,
		})
	}
	for _, name := range n.sortedChildrenNames() {
		n.Children[name].list(l)
	}
}

func (n *Node) reduce() {
	variants := result.VariantSet{}
	for _, child := range n.Children {
		child.reduce()
		for v := range child.Results {
			variants.Add(v)
		}
	}
	for variant := range variants {
		n.promoteCommonStatus(variant)
	}
	for name, child := range n.Children {
		if child.isEmpty() {
			n.removeChild(name)
		}
	}
}

func (n *Node) sortedResultVariants() []result.Variant {
	out := make([]result.Variant, 0, len(n.Results))
	for v := range n.Results {
		out = append(out, v)
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Compare(out[j]) < 0 })
	return out
}

func (n *Node) sortedChildrenNames() []string {
	out := make([]string, 0, len(n.Children))
	for n := range n.Children {
		out = append(out, n)
	}
	sort.Strings(out)
	return out
}

func (n *Node) print(w io.Writer, prefix string) {
	fmt.Fprintf(w, "%v{\n", prefix)
	fmt.Fprintf(w, "%v  name:  '%v'\n", prefix, n.Name)
	fmt.Fprintf(w, "%v  level: '%v'\n", prefix, n.Level)
	for v, s := range n.Results {
		fmt.Fprintf(w, "%v  %v: %v\n", prefix, v, s)
	}
	for _, c := range n.sortedChildrenNames() {
		n.Children[c].print(w, prefix+"  ")
	}
	fmt.Fprintf(w, "%v}\n", prefix)
}

func (n *Node) Format(f fmt.State, verb rune) {
	n.print(f, "")
}

func (n *Node) isEmpty() bool {
	return len(n.Results) == 0 && len(n.Children) == 0
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

func (n *Node) child(name string, level Level) (*Node, error) {
	if n.Children == nil {
		n.Children = map[string]*Node{}
	}
	if existing, ok := n.Children[name]; ok {
		if existing.Level != level {
			return nil, fmt.Errorf("'%v' has conflicting levels for '%v': '%v' and '%v'", n.Name, name, existing.Level, level)
		}
		return existing, nil
	}
	fullname := name
	if level != Suite {
		fullname = fmt.Sprintf("%v%c%v", n.Name, delimiter(n.Level, level), name)
	}
	child := &Node{
		Name:  fullname,
		Level: level,
	}
	n.Children[name] = child
	return child, nil
}

func (n *Node) removeChild(name string) {
	delete(n.Children, name)
	if len(n.Children) == 0 {
		n.Children = nil
	}
}

type Tree struct {
	Node
}

var reSplitTest = regexp.MustCompile(`(\w*):([^:]*)(?::([^:]*):(.*))?`)

// New constructs and returns a Tree from the list of results
func New(results result.List) (*Tree, error) {
	t := Tree{}
	for _, r := range results {
		match := reSplitTest.FindStringSubmatch(r.Name)

		suite := match[1]
		files := strings.Split(match[2], ",")
		tests := strings.Split(match[3], ",")
		kase := match[4]

		if suite == "" {
			return nil, fmt.Errorf("invald test name: '%v'", r.Name)
		}

		node, err := t.child(suite, Suite)
		if err != nil {
			return nil, err
		}
		for _, file := range files {
			if file != "" && file != "*" {
				child, err := node.child(file, File)
				if err != nil {
					return nil, err
				}
				node = child
			}
		}
		for _, test := range tests {
			if test != "" && test != "*" {
				child, err := node.child(test, Test)
				if err != nil {
					return nil, err
				}
				node = child
			}
		}
		if kase != "" && kase != "*" {
			kase = strings.TrimSuffix(kase, ";*")
			child, err := node.child(kase, Case)
			if err != nil {
				return nil, err
			}
			node = child
		}
		if !node.addResult(r.Variant, r.Status) {
			return nil, fmt.Errorf("%v has duplicate results for variant '%v'", node.Name, r.Variant)
		}
	}
	return &t, nil
}

func (t *Tree) Reduce() {
	for _, root := range t.Node.Children {
		root.reduce()
	}
}

func (t *Tree) List() result.List {
	l := result.List{}
	t.list(&l)
	return l
}
