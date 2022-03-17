package container_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/container"
)

func TestNewSet(t *testing.T) {
	s := container.NewSet[string]()
	expectEq(t, "len(s)", len(s), 0)
}

func TestSetFrom(t *testing.T) {
	s := container.SetFrom([]string{"c", "a", "b"})
	expectEq(t, "len(s)", len(s), 3)
}

func TestSetList(t *testing.T) {
	s := container.SetFrom([]string{"c", "a", "b"})
	expectEq(t, "s.List()", s.List(), []string{"a", "b", "c"})
}

func TestSetAdd(t *testing.T) {
	s := container.NewSet[string]()
	s.Add("c")
	expectEq(t, "len(s)", len(s), 1)
	expectEq(t, "s.List()", s.List(), []string{"c"})

	s.Add("a")
	expectEq(t, "len(s)", len(s), 2)
	expectEq(t, "s.List()", s.List(), []string{"a", "c"})

	s.Add("b")
	expectEq(t, "len(s)", len(s), 3)
	expectEq(t, "s.List()", s.List(), []string{"a", "b", "c"})
}

func TestSetRemove(t *testing.T) {
	s := container.SetFrom([]string{"c", "a", "b"})
	s.Remove("c")
	expectEq(t, "len(s)", len(s), 2)
	expectEq(t, "s.List()", s.List(), []string{"a", "b"})

	s.Remove("a")
	expectEq(t, "len(s)", len(s), 1)
	expectEq(t, "s.List()", s.List(), []string{"b"})

	s.Remove("b")
	expectEq(t, "len(s)", len(s), 0)
	expectEq(t, "s.List()", s.List(), []string{})
}

func TestSetAddAll(t *testing.T) {
	s := container.NewSet[string]()
	s.AddAll(container.SetFrom([]string{"c", "a"}))
	expectEq(t, "len(s)", len(s), 2)
	expectEq(t, "s.List()", s.List(), []string{"a", "c"})
}

func TestSetRemoveAll(t *testing.T) {
	s := container.SetFrom([]string{"c", "a", "b"})
	s.RemoveAll(container.SetFrom([]string{"c", "a"}))
	expectEq(t, "len(s)", len(s), 1)
	expectEq(t, "s.List()", s.List(), []string{"b"})
}
