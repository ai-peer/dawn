package result

import (
	"fmt"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

const TagDelimiter = ";"

type Tags struct{ container.Set[string] }

// Returns a new, empty Tags set
func NewTags() Tags {
	return Tags{container.NewSet[string]()}
}

// Returns a Tags set holding the given tags
func TagsFrom(tags ...string) Tags {
	return Tags{container.NewSet(tags...)}
}

func (t Tags) Clone() Tags {
	return Tags{t.Set.Clone()}
}

func (t Tags) String() string {
	return strings.Join(t.List(), TagDelimiter)
}

// Result holds the result of a CTS test
type Result struct {
	Query  query.Query
	Tags   Tags
	Status Status
}

func (r Result) String() string {
	return fmt.Sprintf("%v %v %v", r.Query, r.Tags, r.Status)
}

func Parse(line string) (Result, error) {
	parts := strings.Split(line, " ")
	if len(parts) != 3 {
		return Result{}, fmt.Errorf("unable to parse result '%v'", line)
	}
	q, err := query.Parse(parts[0])
	if err != nil {
		return Result{}, err
	}
	tags := Tags{container.NewSet(strings.Split(parts[1], TagDelimiter)...)}
	s := Status(parts[2])
	return Result{q, tags, s}, nil
}

// List is a list of results
type List []Result

// Returns the list of unique tags.
func (l List) UniqueTags() []Tags {
	tags := container.NewMap[string, Tags]()
	for _, r := range l {
		tags.Add(r.Tags.String(), r.Tags)
	}
	return tags.Values()
}

// ReduceTags returns the reduced list by combining results that have redundant tags.
func (l List) ReduceTags() List {
	// Gather all the tags
	allTags := NewTags()
	uniqueTags := container.NewMap[string, Tags]()
	for _, r := range l {
		allTags.AddAll(r.Tags.Set)
		uniqueTags.Add(r.Tags.String(), r.Tags.Clone())
	}

	removedTags := NewTags()
	for tag := range allTags.Set {
		canRemoveTag := true

		for _, tags := range uniqueTags {
			filter := tags.Clone()
			filter.Remove(tag)
			filtered := l.FilterByTags(filter)

			statusByQuery := map[query.Query]Status{}
			for _, r := range filtered {
				if existing, ok := statusByQuery[r.Query]; ok {
					if existing != r.Status {
						canRemoveTag = false
						break
					}
				} else {
					statusByQuery[r.Query] = r.Status
				}
			}
		}

		if canRemoveTag {
			removedTags.Add(tag)

			newUniqueTags := container.NewMap[string, Tags]()
			for _, tags := range uniqueTags {
				tags.Remove(tag)
				newUniqueTags.Add(tags.String(), tags)
			}
			uniqueTags = newUniqueTags
		}
	}

	if len(removedTags.Set) == 0 {
		return l // Nothing could be reduced
	}

	out := l.TransformTags(func(tags Tags) Tags {
		tags.RemoveAll(removedTags.Set)
		return tags
	})
	if err := out.RemoveDuplicates(); err != nil {
		panic(err)
	}
	return out
}

func (l List) TransformTags(f func(Tags) Tags) List {
	cache := map[string]Tags{}
	out := List{}
	for _, r := range l {
		key := r.Tags.String()
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

func (l *List) RemoveDuplicates() error {
	type Key struct {
		query query.Query
		tags  string
	}
	seen := map[Key]Status{}
	reduced := make(List, 0, len(*l))
	for _, r := range *l {
		key := Key{r.Query, r.Tags.String()}
		if existing, ok := seen[key]; ok {
			if existing != r.Status {
				return fmt.Errorf("list contains two results for '%v' '%v': %v and %v", r.Query, r.Tags, existing, r.Status)
			}
		} else {
			seen[key] = r.Status
			reduced = append(reduced, r)
		}
	}
	*l = reduced
	return nil
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

// Filter returns the results that match the given predicate
func (l List) FilterByTags(tags Tags) List {
	return l.Filter(func(r Result) bool {
		return r.Tags.ContainsAll(tags.Set)
	})
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
