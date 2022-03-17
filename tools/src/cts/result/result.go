package result

import (
	"fmt"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

const TagDelimiter = ";"

type Tags = container.Set[string]

// Returns a new tag set with the given tags
func NewTags(tags ...string) Tags {
	return Tags(container.NewSet(tags...))
}

func TagsToString(t Tags) string {
	return strings.Join(t.List(), TagDelimiter)
}

// Result holds the result of a CTS test
type Result struct {
	Query  query.Query
	Tags   Tags
	Status Status
}

func (r Result) String() string {
	return fmt.Sprintf("%v %v %v", r.Query, TagsToString(r.Tags), r.Status)
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
	tags := NewTags(strings.Split(parts[1], TagDelimiter)...)
	s := Status(parts[2])
	return Result{q, tags, s}, nil
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

// ReduceTags returns the reduced list by combining results that have redundant tags.
func (l List) ReduceTags() List {
	// Gather all the tags
	allTags := NewTags()
	uniqueTags := container.NewMap[string, Tags]()
	for _, r := range l {
		allTags.AddAll(r.Tags)
		uniqueTags.Add(TagsToString(r.Tags), r.Tags.Clone())
	}

	removedTags := NewTags()
	for tag := range allTags {
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
				newUniqueTags.Add(TagsToString(tags), tags)
			}
			uniqueTags = newUniqueTags
		}
	}

	if len(removedTags) == 0 {
		return l // Nothing could be reduced
	}

	out := l.TransformTags(func(tags Tags) Tags {
		tags.RemoveAll(removedTags)
		return tags
	})
	out = out.ReplaceDuplicates(func(l List) Status {
		statuses := l.Statuses()
		if len(statuses) != 1 {
			panic(fmt.Errorf("result list unexpectedly contains status collisions"))
		}
		for status := range statuses {
			return status
		}
		return Unknown // unreachable
	})
	return out
}

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

// Filter returns the results that match the given predicate
func (l List) FilterByTags(tags Tags) List {
	return l.Filter(func(r Result) bool {
		return r.Tags.ContainsAll(tags)
	})
}

// Statuses returns a set of all the statuses in the list
func (l List) Statuses() container.Set[Status] {
	set := container.NewSet[Status]()
	for _, r := range l {
		set.Add(r.Status)
	}
	return set
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
