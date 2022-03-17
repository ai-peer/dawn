// Package set implements a basic generic unordered set using a map
package set

type ordered interface {
	~int | ~int8 | ~int16 | ~int32 | ~int64 | ~uint | ~uint8 | ~uint16 |
		~uint32 | ~uint64 | ~uintptr | ~float32 | ~float64 | ~string
}

type item interface {
	comparable
	ordered
}

// A generic set structure
type Set[T comparable] map[T]struct{}

// Returns a new empty set
func New[T comparable]() Set[T] {
	return make(Set[T])
}

// Returns a new set populated with the given slice
func From[T comparable](slice []T) Set[T] {
	s := make(Set[T], len(slice))
	for _, item := range slice {
		s.Add(item)
	}
	return s
}

// Add adds an item to the set
func (s Set[T]) Add(item T) {
	s[item] = struct{}{}
}

// Remove removes an item from the set
func (s Set[T]) Remove(item T) {
	delete(s, item)
}

// Contains returns true if the set contains the given item
func (s Set[T]) Contains(item T) bool {
	_, found := s[item]
	return found
}

// List returns the unsorted entries of the set as a slice
func (s Set[T]) List() []T {
	out := make([]T, 0, len(s))
	for v := range s {
		out = append(out, v)
	}
	return out
}
