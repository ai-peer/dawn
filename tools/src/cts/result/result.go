package result

import (
	"fmt"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

// Result holds the result of a CTS test
type Result struct {
	Query  query.Query
	Tags   Tags
	Status Status
}

// Format writes the Result to the fmt.State
func (r Result) Format(f fmt.State, verb rune) {
	if len(r.Tags) > 0 {
		fmt.Fprintf(f, "%v %v %v", r.Query, TagsToString(r.Tags), r.Status)
	} else {
		fmt.Fprintf(f, "%v %v", r.Query, r.Status)
	}
}

func (r Result) String() string {
	sb := strings.Builder{}
	fmt.Fprint(&sb, r)
	return sb.String()
}

func Parse(line string) (Result, error) {
	token := func() string {
		for i, c := range line {
			if c != ' ' {
				line = line[i:]
				break
			}
		}
		for i, c := range line {
			if c == ' ' {
				tok := line[:i]
				line = line[i:]
				return tok
			}
		}
		tok := line
		line = ""
		return tok
	}

	a := token()
	b := token()
	c := token()
	if a == "" || b == "" {
		return Result{}, fmt.Errorf("unable to parse result '%v'", line)
	}
	q, err := query.Parse(a)
	if err != nil {
		return Result{}, err
	}
	if c == "" {
		status := Status(b)
		return Result{q, nil, status}, nil
	}
	tags := NewTags(strings.Split(b, TagDelimiter)...)
	status := Status(c)
	return Result{q, tags, status}, nil
}

// List is a list of results
type List []Result

// Returns the list of unique tags.
func (l List) UniqueTags() []Tags {
	tags := container.NewMap[string, Tags]()
	for _, r := range l {
		tags.Add(TagsToString(r.Tags), r.Tags)
	}
	return tags.Values()
}

// TransformTags returns the list of results with the tags transformed using f.
// TransformTags assumes that f will return the same output for the same input.
func (l List) TransformTags(f func(Tags) Tags) List {
	cache := map[string]Tags{}
	out := List{}
	for _, r := range l {
		key := TagsToString(r.Tags)
		tags, cached := cache[key]
		if !cached {
			tags = f(r.Tags.Clone())
			cache[key] = tags
		}
		out = append(out, Result{
			Query:  r.Query,
			Tags:   tags,
			Status: r.Status,
		})
	}
	return out
}

// ReplaceDuplicates returns a new list with duplicate test results replaced.
// When a duplicate is found, the function f is called with the duplicate
// results. The returned status will be used as the replaced result.
func (l List) ReplaceDuplicates(f func(List) Status) List {
	type key struct {
		query query.Query
		tags  string
	}
	m := map[key]List{}
	for _, r := range l {
		k := key{r.Query, TagsToString(r.Tags)}
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
		k := key{r.Query, TagsToString(r.Tags)}
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
		ta := strings.Join(a.Tags.List(), TagDelimiter)
		tb := strings.Join(b.Tags.List(), TagDelimiter)
		switch {
		case ta < tb:
			return true
		case ta > tb:
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

// FilterByTags returns the results that have all the given tags
func (l List) FilterByTags(tags Tags) List {
	return l.Filter(func(r Result) bool {
		return r.Tags.ContainsAll(tags)
	})
}
