package container

import (
	"sort"
)

// A generic set structure
type Set[T key] map[T]struct{}

// Returns a new empty set
func NewSet[T key]() Set[T] {
	return make(Set[T])
}

// Returns a new set populated with the given slice
func SetFrom[T key](slice []T) Set[T] {
	s := make(Set[T], len(slice))
	for _, item := range slice {
		s.Add(item)
	}
	return s
}

// Add adds an item to the set.
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

// Contains returns true if the set contains all the items in o
func (s Set[T]) ContainsAll(o Set[T]) bool {
	for item := range o {
		if !s.Contains(item) {
			return false
		}
	}
	return true
}

// List returns the sorted entries of the set as a slice
func (s Set[T]) List() []T {
	out := make([]T, 0, len(s))
	for v := range s {
		out = append(out, v)
	}
	sort.Slice(out, func(i, j int) bool { return out[i] < out[j] })
	return out
}
