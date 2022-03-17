package result

type GPU string
type OS string

type Variant struct {
	GPU   GPU
	OS    OS
	Flags string
}

func (v Variant) Compare(o Variant) int {
	switch {
	case v.GPU < o.GPU:
		return -1
	case v.GPU > o.GPU:
		return 1
	case v.OS < o.OS:
		return -1
	case v.OS > o.OS:
		return 1
	case v.Flags < o.Flags:
		return -1
	case v.Flags > o.Flags:
		return 1
	}
	return 0
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
