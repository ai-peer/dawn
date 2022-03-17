package result

type GPU string
type OS string

type Variant struct {
	GPU GPU
	OS  OS
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
	}
	return 0
}

type VariantSet map[Variant]struct{}

func (s VariantSet) Add(v Variant) {
	s[v] = struct{}{}
}

func (s VariantSet) Remove(v Variant) {
	delete(s, v)
}
