package expectations

import (
	"errors"
	"fmt"
	"log"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
)

type Diagnostic struct {
	Line    int
	Message string
}

const consumed result.Status = "<<consumed>>"

// A query tree, without status. Used for filtering the full result set to sub-trees.
type queryTree struct {
	results        result.List
	resultsByQuery map[query.Query][]int // Indices on results
	tree           tree.Tree
}

func newQueryTree(results result.List) queryTree {
	resultsByQuery := map[query.Query][]int{}
	for i, r := range results {
		l := resultsByQuery[r.Query]
		l = append(l, i)
		resultsByQuery[r.Query] = l
	}

	tree := tree.Tree{}
	for _, r := range results {
		// We intentionally ignore errors here.
		// The only error is ErrDuplicateResult, and we know there might be
		// duplicates as this is the tree union across all tags.
		tree.Add(result.Result{Query: r.Query, Status: "<no-status>"})
	}

	return queryTree{results, resultsByQuery, tree}
}

func (qt *queryTree) resultsFor(q query.Query) (result.List, error) {
	l, err := qt.tree.Collect(q)
	if err != nil {
		return nil, err
	}

	out := result.List{}
	for _, r := range l {
		for _, idx := range qt.resultsByQuery[r.Query] {
			out = append(out, qt.results[idx])
		}
	}

	return out, nil
}

func (qt *queryTree) markAsConsumed(r result.Result) {
	tags := r.Tags.String()
	for _, idx := range qt.resultsByQuery[r.Query] {
		if qt.results[idx].Tags.String() == tags {
			qt.results[idx].Status = consumed
		}
	}
}

// Returns results with all non-expectation tags removed, and stripping all but
// the highest priority tag from each tag set. The tag sets are defined by the
// the `BEGIN TAG HEADER` / `END TAG HEADER` section at the top of the
// expectations file.
func (c *Content) filterResultTags(results result.List) result.List {
	return results.TransformTags(func(t result.Tags) result.Tags {
		type HighestPrioritySetTag struct {
			tag      string
			priority int
		}
		// Set name to highest priority tag for that set
		best := map[string]HighestPrioritySetTag{}
		for tag := range t.Set {
			sp, ok := c.Tags[tag]
			if ok {
				if set := best[sp.Set]; sp.Priority >= set.priority {
					best[sp.Set] = HighestPrioritySetTag{tag, sp.Priority}
				}
			}
		}
		t = result.NewTags()
		for _, ts := range best {
			t.Add(ts.tag)
		}
		return t
	})
}

func (c *Content) Update(results result.List, cfg *Config) ([]Diagnostic, error) {
	// Remove all cases. The runner cannot handle these.
	// for _, r := range results {
	// 	r.Query.Cases = ""
	// }
	// results.ReplaceDuplicates(func(l result.List) result.Status {
	// 	statuses := l.Statuses()
	// 	switch {
	// 	case statuses.Contains(result.Crash):
	// 		return result.Crash
	// 	case statuses.Contains(result.Failure):
	// 		return result.Failure
	// 	case statuses.Contains(result.Skip):
	// 		return result.Skip
	// 	case len(statuses) == 1 && statuses.Contains(result.Pass):
	// 		return result.Pass
	// 	default:
	// 		return result.Unknown
	// 	}
	// })

	if c.Tags != nil {
		log.Println("Filtering tags...")
		results = c.filterResultTags(results)
	}

	log.Println("Building result tree...")
	qt := newQueryTree(results)

	u := updater{
		in:        *c,
		out:       Content{},
		cfg:       cfg,
		qt:        qt,
		tagsInUse: gatherTagsInUse(results, c.Tags),
	}

	log.Println("Updating expectations...")
	if err := u.build(); err != nil {
		return nil, err
	}
	*c = u.out
	return u.diags, nil
}

type updater struct {
	in, out   Content
	cfg       *Config
	qt        queryTree
	tagsInUse result.Tags
	diags     []Diagnostic
}

func gatherTagsInUse(results result.List, tagSets map[string]TagSetAndPriority) result.Tags {
	// Construct a map of set name -> tags in use
	sets := map[string]result.Tags{}
	// Construct the Tags for each set
	for _, set := range tagSets {
		if sets[set.Set].Set == nil {
			sets[set.Set] = result.NewTags()
		}
	}
	// Traverse the results, gathering the tags, binning them by set, and
	// populating sets.
	{
		seen := container.NewSet[string]()
		for _, r := range results {
			key := r.Tags.String()
			if !seen.Contains(key) {
				seen.Add(key)
				for tag := range r.Tags.Set {
					if set, ok := tagSets[tag]; ok {
						sets[set.Set].Add(tag)
					}
				}
			}
		}
	}
	tagsInUse := result.NewTags()
	for _, tags := range sets {
		// If we only ever use the same tag in the same set, then we can
		// filter this away, as the tag is not helping filter.
		if len(tags.Set) > 1 {
			tagsInUse.Set.AddAll(tags.Set)
		}
	}
	return tagsInUse
}

func (u *updater) build() error {
	// Update all the existing chunks
	for _, chunk := range u.in.Chunks {
		out, err := u.chunk(chunk)
		if err != nil {
			return err
		}
		if out.IsBlankLine() {
			u.out.MaybeAddBlankLine()
			continue
		}
		u.out.Chunks = append(u.out.Chunks, out)
	}

	// Emit new expectations
	if err := u.addNewExpectations(); err != nil {
		return err
	}

	return nil
}

func (u *updater) addNewExpectations() error {
	results := u.qt.results

	newExpectations := []Expectation{}
	for _, tags := range results.UniqueTags() {
		t, err := tree.New(results.FilterByTags(tags))
		if err != nil {
			return fmt.Errorf("while adding new expectations for tags '%v': %w", tags, err)
		}
		t.Reduce()

		results := t.AllResults().Filter(func(r result.Result) bool {
			return r.Status != result.Pass && r.Status != consumed
		})
		for _, r := range results {
			tags := result.Tags{Set: tags.Set.Intersection(u.tagsInUse.Set)}
			newExpectations = append(newExpectations, Expectation{
				Bug:    "crbug.com/dawn/0000",
				Tags:   tags,
				Test:   trimCaseWildcard(r.Query).String(),
				Status: []string{string(r.Status)},
			})
		}
	}

	if len(newExpectations) > 0 {
		u.out.MaybeAddBlankLine()
		u.out.Chunks = append(u.out.Chunks, Chunk{
			Comments:     []string{"# New expectations. Please triage:"},
			Expectations: newExpectations,
		})
	}
	return nil
}

func (u *updater) chunk(in Chunk) (Chunk, error) {
	if len(in.Expectations) == 0 {
		// Just a comment / blank line
		return in, nil
	}

	out := Chunk{Comments: in.Comments}
	for _, exIn := range in.Expectations {
		exOut, err := u.expectation(exIn)
		if err != nil {
			return Chunk{}, err
		}
		out.Expectations = append(out.Expectations, exOut...)
	}

	return out, nil
}

func (u *updater) expectation(in Expectation) ([]Expectation, error) {
	diag := func(msg string, args ...interface{}) {
		u.diags = append(u.diags, Diagnostic{in.Line, fmt.Sprintf(msg, args...)})
	}

	q, err := query.Parse(in.Test)
	if err != nil {
		diag("%v", err)
		return nil, nil
	}

	results, err := u.qt.resultsFor(q)
	if err != nil {
		if errors.As(err, &tree.ErrNoResultForQuery{}) {
			diag("no tests with query '%v'", in.Test)
			return nil, nil
		}
		return nil, err
	}

	results = results.FilterByTags(in.Tags)

	// Mark these tests as consumed
	for _, r := range results {
		u.qt.markAsConsumed(r)
	}

	out := []Expectation{}
	for _, tags := range results.UniqueTags() {
		results := results.FilterByTags(tags)

		// Build a tree with just the results of the given unique set of tags
		t, err := tree.New(results)
		if err != nil {
			return nil, err
		}
		// Reduce the results
		t.ReduceTo(q)
		// Collect the reduced results
		reduced := t.AllResults()
		// Filter the results to just those that are non-passing
		reduced = reduced.Filter(func(r result.Result) bool {
			return r.Status != result.Pass && r.Status != consumed
		})

		tags := result.Tags{Set: tags.Intersection(u.tagsInUse.Set)}

		for _, r := range reduced {
			// Emit the updated expectations
			out = append(out, Expectation{
				Line:   in.Line,
				Bug:    in.Bug,
				Tags:   tags,
				Test:   trimCaseWildcard(r.Query).String(),
				Status: []string{string(r.Status)},
			})
		}
	}

	return out, nil
}

func trimCaseWildcard(q query.Query) query.Query {
	q.Cases = strings.TrimSuffix(q.Cases, ";*")
	return q
}
