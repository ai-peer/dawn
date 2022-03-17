package result

import (
	"fmt"
	"regexp"
	"sort"
	"strings"
)

type GPU string
type OS string

type Variant struct {
	OS    OS
	GPU   GPU
	Flags string `json:",omitempty"`
}

type Variants []Variant

// Sort sorts the list
func (l Variants) Sort() {
	sort.Slice(l, func(i, j int) bool { return l[i].Compare(l[j]) < 0 })
}

func (v Variant) String() string {
	if v.Flags != "" {
		return fmt.Sprintf("[OS: '%v', GPU: '%v', Flags: '%v']", v.OS, v.GPU, v.Flags)
	}
	return fmt.Sprintf("[OS: '%v', GPU: '%v']", v.OS, v.GPU)
}

func (v Variant) Compare(o Variant) int {
	switch {
	case v.OS < o.OS:
		return -1
	case v.OS > o.OS:
		return 1
	case v.GPU < o.GPU:
		return -1
	case v.GPU > o.GPU:
		return 1
	case v.Flags < o.Flags:
		return -1
	case v.Flags > o.Flags:
		return 1
	}
	return 0
}

var reParseVariantField = regexp.MustCompile(`(\S+): *'([^']*)'`)

func ParseVariant(s string) (Variant, error) {
	s = strings.TrimSpace(s)
	if len(s) < 2 || s[0] != '[' || s[len(s)-1] != ']' {
		return Variant{}, fmt.Errorf("failed to parse variant: '%v'", s)
	}
	s = s[1 : len(s)-1]
	v := Variant{}
	for _, m := range reParseVariantField.FindAllStringSubmatch(s, -1) {
		if len(m) == 3 {
			switch m[1] {
			case "OS":
				v.OS = OS(m[2])
			case "GPU":
				v.GPU = GPU(m[2])
			case "Flags":
				v.Flags = m[2]
			}
		}
	}
	return v, nil
}

// VariantSet is a set of Variants
type VariantSet map[Variant]struct{}

// Add adds the Variant v to the set
func (s VariantSet) Add(v Variant) {
	s[v] = struct{}{}
}

// Remove removes the Variant v to the set
func (s VariantSet) Remove(v Variant) {
	delete(s, v)
}

// List returns the entries in the set as a sorted list
func (s VariantSet) List() Variants {
	out := make(Variants, 0, len(s))
	for v := range s {
		out = append(out, v)
	}
	out.Sort()
	return out
}
