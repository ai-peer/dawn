// Package mph provides utilities for generating minimal perfect hashes for short strings
package mph

import (
	"dawn.googlesource.com/dawn/tools/src/container"
)

const prime = 83591

// Uint64 perfect hash function
// bin = (word * multiplier) % len(Bin)
type HashTree struct {
	Multiplier uint64
	Bins       []Bin
	Prime      uint64
}

type Bin struct {
	U64      uint64
	Word     string // The substring
	String   string // The full string
	Children *HashTree
}

func u64LE(str string) uint64 {
	n := len(str)
	if n > 8 {
		n = 8
	}
	u64 := uint64(0)
	for i := n - 1; i >= 0; i-- {
		u64 = (u64 << 8) | uint64(str[i])
	}
	return u64
}

func Find(inputStrings container.Set[string]) *HashTree {
	return find(inputStrings, 0)
}

func find(inputStrings container.Set[string], offset int) *HashTree {
	u64s := container.NewMap[uint64, []string]()
	for str := range inputStrings {
		u64 := u64LE(str[offset:])
		u64s[u64] = append(u64s[u64], str)
	}

	multiplier := search(u64s.Keys())
	modulo := len(u64s)
	tree := HashTree{Multiplier: multiplier, Bins: make([]Bin, len(u64s)), Prime: prime}

	for u64, strs := range u64s {
		binIdx := hash(u64, multiplier, uint64(modulo))
		bin := &tree.Bins[binIdx]
		bin.U64 = u64
		if len(strs) > 1 {
			bin.Children = find(container.NewSet(strs...), offset+8)
		} else {
			bin.String = strs[0]
			bin.Word = bin.String[offset:]
		}
	}
	return &tree
}

func search(keys []uint64) uint64 {
	if len(keys) > 63 {
		panic("minimal perfect hash search not implemented for more than 63 keys")
	}
	modulo := uint64(len(keys))
	test := (uint64(1) << len(keys)) - 1
	for m := uint64(1); m < 0xffffffff; m++ {
		mask := uint64(0)
		for _, key := range keys {
			mask |= 1 << hash(key, m, modulo)
		}
		if mask == test {
			return m
		}
	}
	panic("no perfect hash found")
	return 0
}

func hash(value, multiplier, modulo uint64) uint64 {
	return ((value * multiplier) % prime) % uint64(modulo)
}
