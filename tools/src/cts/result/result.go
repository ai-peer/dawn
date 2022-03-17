package result

import (
	"fmt"
	"regexp"
	"sort"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

// Result holds the result of a CTS test
type Result struct {
	Query   query.Query
	Variant Variant
	Status  Status
}

func (r Result) String() string {
	return fmt.Sprintf("%v %v %v", r.Query, r.Variant, r.Status)
}

var reParse = regexp.MustCompile(`^([^ ]+) (\[[^\]]*\]) (.*)$`)

func Parse(line string) (Result, error) {
	match := reParse.FindStringSubmatch(line)
	if len(match) != 4 {
		return Result{}, fmt.Errorf("unable to parse result '%v'", line)
	}
	q, err := query.Parse(match[1])
	if err != nil {
		return Result{}, err
	}
	v, err := ParseVariant(match[2])
	if err != nil {
		return Result{}, err
	}
	return Result{q, v, Status(match[3])}, nil
}

// List is a list of results
type List []Result

// ReplaceDuplicates returns a new list with duplicate test results replaced.
// When a duplicate is found, the function f is called with the duplicate
// results. The returned status will be used as the replaced result.
func (l List) ReplaceDuplicates(f func(List) Status) List {
	type key struct {
		query   query.Query
		variant Variant
	}
	m := map[key]List{}
	for _, r := range l {
		k := key{r.Query, r.Variant}
		m[k] = append(m[k], r)
	}
	for key, results := range m {
		if len(results) > 1 {
			result := results[0]
			result.Status = f(results)
			m[key] = List{result}
		}
	}
	out := make(List, 0, len(m))
	for _, r := range l {
		k := key{r.Query, r.Variant}
		if unique, ok := m[k]; ok {
			out = append(out, unique[0])
			delete(m, k)
		}
	}
	return out
}

// Sort sorts the list
func (l List) Sort() {
	sort.Slice(l, func(i, j int) bool {
		a, b := l[i], l[j]
		switch a.Query.Compare(b.Query) {
		case -1:
			return true
		case 1:
			return false
		}
		switch a.Variant.Compare(b.Variant) {
		case -1:
			return true
		case 1:
			return false
		}
		return a.Status < b.Status
	})
}

// Filter returns the results that match the given predicate
func (l List) Filter(f func(Result) bool) List {
	out := make(List, 0, len(l))
	for _, r := range l {
		if f(r) {
			out = append(out, r)
		}
	}
	return out
}

// All returns true if all the results have the provided status
func (l List) All(s Status) bool {
	for _, r := range l {
		if r.Status != s {
			return false
		}
	}
	return true
}

// Any returns true if any the results have the provided status
func (l List) Any(s Status) bool {
	for _, r := range l {
		if r.Status == s {
			return true
		}
	}
	return false
}

// Variants returns the unique variants in use by all the results
func (l List) Variants() Variants {
	s := VariantSet{}
	for _, r := range l {
		s.Add(r.Variant)
	}
	return s.List()
}
