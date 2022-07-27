// Package mph provides utilities for generating minimal perfect hashes for short strings
package mph

import (
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
)

const prime = 83591

// Uint64 perfect hash function
// bin = ((word * multiplier) % prime) & mask
type Node struct {
	Multiplier uint64
	Bins       []Bin
	Prime      uint64
	Mask       uint64
}

type Bin struct {
	U64      uint64
	String   string
	Children *Node
}

func u64LE(str string) uint64 {
	u64 := uint64(0)
	for i := len(str) - 1; i >= 0; i-- {
		u64 = (u64 << 8) | uint64(str[i])
	}
	return u64
}

func Find(inputStrings container.Set[string]) *Node {
	// Null-terminate strings
	nullTerminated := make(container.Set[string], len(inputStrings))
	for str := range inputStrings {
		nullTerminated.Add(str + "\x00")
	}
	return find(nullTerminated, 0)
}

func find(inputStrings container.Set[string], offset int) *Node {
	type binned struct {
		str      string   // The full string
		word     string   // The next 8 bytes
		key      uint64   // The next 8 bytes as u64
		children []string // Child strings > 8 bytes
	}
	// group together the strings that have the same next 8 bytes
	bins := container.NewMap[uint64, binned]()
	for str := range inputStrings {
		word := str[offset:]
		if len(word) > 8 {
			word = word[:8]
		}
		key := u64LE(word)
		bin, found := bins[key]
		if len(str) > offset+8 {
			bin.children = append(bin.children, str)
		}
		if !found {
			bin.word = word
			bin.key = key
			bin.str = str
		}
		bins[key] = bin
	}

	multiplier := search(bins.Keys())
	mask := nextPOT(uint64(len(bins))) - 1
	node := Node{
		Multiplier: multiplier,
		Bins:       make([]Bin, len(bins)),
		Prime:      prime,
		Mask:       mask,
	}
	for _, bin := range bins {
		binIdx := hash(bin.key, multiplier, mask)
		b := &node.Bins[binIdx]
		b.U64 = bin.key
		b.String = strings.TrimSuffix(bin.str, "\x00")
		if len(bin.children) > 0 {
			b.Children = find(container.NewSet(bin.children...), offset+8)
		}
	}
	return &node
}

func search(keys []uint64) uint64 {
	if len(keys) > 63 {
		panic("minimal perfect hash search not implemented for more than 63 keys")
	}
	mask := nextPOT(uint64(len(keys))) - 1
	test := (uint64(1) << len(keys)) - 1
	for m := uint64(1); m < 0xffffffff; m++ {
		bits := uint64(0)
		for _, key := range keys {
			bits |= 1 << hash(key, m, mask)
		}
		if bits == test {
			return m
		}
	}
	panic("no perfect hash found")
}

func hash(value, multiplier, mask uint64) uint64 {
	return ((value * multiplier) % prime) & mask
}

func nextPOT(i uint64) uint64 {
	i--
	i |= i >> 1
	i |= i >> 2
	i |= i >> 4
	i |= i >> 8
	i |= i >> 16
	i |= i >> 32
	i++
	return i
}
