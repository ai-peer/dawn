package expectations

import (
	"errors"
	"fmt"
	"log"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type Diagnostic struct {
	Line    int
	Message string
}

const consumed result.Status = "<<consumed>>"

// A query tree, without status. Used for filtering the full result set to sub-trees.
type queryTree struct {
	results        result.List
	consumedAt     []int
	resultsByQuery map[query.Query][]int // Indices on results
	tree           query.Tree[struct{}]
}

func newQueryTree(results result.List) queryTree {
	log.Println("Building query tree...")

	resultsByQuery := map[query.Query][]int{}
	for i, r := range results {
		l := resultsByQuery[r.Query]
		l = append(l, i)
		resultsByQuery[r.Query] = l
	}

	t := query.Tree[struct{}]{}
	for _, r := range results {
		// We intentionally ignore errors here.
		// The only error is ErrDuplicateResult, and we know there might be
		// duplicates as this is the tree union across all tags.
		t.Add(r.Query, struct{}{})
	}

	consumedAt := make([]int, len(results))
	return queryTree{results, consumedAt, resultsByQuery, t}
}

func (qt *queryTree) resultsFor(q query.Query) (result.List, error) {
	l, err := qt.tree.Glob(q)
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

func (qt *queryTree) markAsConsumed(r result.Result, line int) error {
	tags := result.TagsToString(r.Tags)
	for _, idx := range qt.resultsByQuery[r.Query] {
		if result.TagsToString(qt.results[idx].Tags) == tags {
			if at := qt.consumedAt[idx]; at > 0 {
				return fmt.Errorf("'%v' already has expectation at line %v", r.Query, at)
			}
			qt.results[idx].Status = consumed
			qt.consumedAt[idx] = line
		}
	}
	return nil
}

func (c *Content) Update(results result.List, cfg *Config) ([]Diagnostic, error) {
	// Make a copy of the results. This code mutates the list.
	results = append(result.List{}, results...)

	for i, r := range results {
		switch r.Status {
		case result.Pass, result.RetryOnFailure, result.Skip:
			// keep
		default:
			// note: We're pessimistically converting Slow to failure, as many
			// slow tests end up failing anyway.
			results[i].Status = result.Failure
		}
	}

	u := updater{
		in:       *c,
		out:      Content{},
		cfg:      cfg,
		qt:       newQueryTree(results),
		variants: gatherVariants(results, c.Tags),
	}

	log.Println("Updating expectations...")
	if err := u.build(); err != nil {
		return nil, err
	}
	*c = u.out
	return u.diags, nil
}

type updater struct {
	in, out  Content
	cfg      *Config
	qt       queryTree
	variants []result.Tags
	diags    []Diagnostic
}

func gatherVariants(results result.List, tagSets map[string]TagSetAndPriority) []result.Tags {
	log.Println("Gathering unique variants...")

	// Gather the full set of unique tags
	variants := results.UniqueTags()
	// Construct a map of set name -> tags in use
	sets := map[string]result.Tags{}
	// Examine all the variant tags, bin them by tag set.
	for _, variant := range variants {
		for tag := range variant {
			if set, ok := tagSets[tag]; ok {
				if sets[set.Set] == nil {
					sets[set.Set] = result.NewTags()
				}
				sets[set.Set].Add(tag)
			}
		}
	}
	// If a tag set has more than one tag in use, then we need to include these
	// in the variants.
	tagsInUse := result.NewTags()
	for _, tags := range sets {
		if len(tags) > 1 {
			tagsInUse.AddAll(tags)
		}
	}
	// Filter away unused tags from the variants
	for i, variant := range variants {
		variants[i] = variant.Intersection(tagsInUse)
	}
	return variants
}

func (u *updater) build() error {
	// Update all the existing chunks
	for _, in := range u.in.Chunks {
		out, err := u.chunk(in)
		if err != nil {
			return err
		}
		// If all chunk had expectations, but now they've gone, remove the chunk
		if len(in.Expectations) > 0 && len(out.Expectations) == 0 {
			continue
		}
		if out.IsBlankLine() {
			u.out.MaybeAddBlankLine()
			continue
		}
		u.out.Chunks = append(u.out.Chunks, out)
	}

	// Emit new expectations
	if err := u.addNewExpectations(); err != nil {
		return fmt.Errorf("failed to add new expectations: %w", err)
	}

	return nil
}

func (u *updater) chunk(in Chunk) (Chunk, error) {
	if len(in.Expectations) == 0 {
		// Just a comment / blank line
		return in, nil
	}

	keep := false
	for _, l := range in.Comments {
		if strings.Contains(l, "KEEP") {
			keep = true
			break
		}
	}

	out := Chunk{Comments: in.Comments}
	for _, exIn := range in.Expectations {
		exOut, err := u.expectation(exIn, keep)
		if err != nil {
			return Chunk{}, err
		}
		out.Expectations = append(out.Expectations, exOut...)
	}

	return out, nil
}

func (u *updater) expectation(in Expectation, keep bool) ([]Expectation, error) {
	diag := func(msg string, args ...interface{}) {
		u.diags = append(u.diags, Diagnostic{in.Line, fmt.Sprintf(msg, args...)})
	}

	q := query.Parse(in.Test)

	// Grab all the results that match the query
	unfiltered, err := u.qt.resultsFor(q)
	if err != nil {
		if errors.As(err, &query.ErrNoDataForQuery{}) {
			diag("no results found for '%v'", in.Test)
			return []Expectation{in}, nil
		}
		return nil, err
	}

	if len(unfiltered) == 0 {
		diag("no results found for '%v'", in.Test)
		return []Expectation{in}, nil
	}

	// Filter these results to the expectation's tags
	unreduced := unfiltered.FilterByTags(in.Tags)

	// Mark all the results as consumed
	unreduced = unreduced.Filter(func(r result.Result) bool {
		if err := u.qt.markAsConsumed(r, in.Line); err != nil {
			diag("%v", err)
			return false
		}
		return true
	})

	if keep {
		return []Expectation{in}, nil
	}

	results := result.List{}

	// For each variant...
	for _, variant := range u.variants {
		// Build a tree and reduce it
		t, err := unreduced.FilterByTags(variant).StatusTree()
		if err != nil {
			return nil, fmt.Errorf("while building tree for tags '%v': %w", variant, err)
		}
		t.ReduceTo(q, result.CommonStatus)
		for _, qs := range t.List() {
			results = append(results, result.Result{
				Query:  qs.Query,
				Tags:   variant,
				Status: qs.Data,
			})
		}
	}

	// Filter out any results that are either Pass or we've already consumed
	results = results.Filter(func(r result.Result) bool {
		return r.Status != result.Pass && r.Status != consumed
	})

	return u.resultsToExpectations(results, in.Bug), nil
}

func (u *updater) addNewExpectations() error {
	results := result.List{}

	// For each variant...
	for _, variant := range u.variants {
		// Build a tree and reduce it
		t, err := u.qt.results.FilterByTags(variant).StatusTree()
		if err != nil {
			return fmt.Errorf("while building tree for tags '%v': %w", variant, err)
		}
		t.Reduce(result.CommonStatus)
		for _, qs := range t.List() {
			results = append(results, result.Result{
				Query:  qs.Query,
				Tags:   variant,
				Status: qs.Data,
			})
		}
	}

	// Filter out any results that are either Pass or we've already consumed
	results = results.Filter(func(r result.Result) bool {
		return r.Status != result.Pass && r.Status != consumed
	})

	flakes, failures := result.List{}, result.List{}
	for _, r := range results {
		if r.Status == result.RetryOnFailure {
			flakes = append(flakes, r)
		} else {
			failures = append(failures, r)
		}
	}

	flakes.Sort()
	failures.Sort()

	for _, set := range []struct {
		results result.List
		comment string
	}{
		{flakes, "# New flakes. Please triage:"},
		{failures, "# New failures. Please triage:"},
	} {
		newExpectations := u.resultsToExpectations(set.results, "crbug.com/dawn/0000")
		if len(newExpectations) > 0 {
			u.out.MaybeAddBlankLine()
			u.out.Chunks = append(u.out.Chunks, Chunk{
				Comments:     []string{set.comment},
				Expectations: newExpectations,
			})
		}
	}

	return nil
}

func (u *updater) resultsToExpectations(results result.List, bug string) []Expectation {
	// Transform results to remove multiple tags from the same tag-set
	results = u.filterResultTags(results)

	results.Sort()

	out := make([]Expectation, len(results))
	for i, r := range results {
		q := r.Query.String()
		if r.Query.Target() == query.Tests && !r.Query.IsWildcard() {
			// The expectation validator wants a trailing ':' for test queries
			q += query.TargetDelimiter
		}
		out[i] = Expectation{
			Bug:    bug,
			Tags:   r.Tags,
			Test:   q,
			Status: []string{string(r.Status)},
		}
	}

	return out
}

// Returns results with all non-expectation tags removed, and stripping all but
// the highest priority tag from each tag set. The tag sets are defined by the
// the `BEGIN TAG HEADER` / `END TAG HEADER` section at the top of the
// expectations file.
func (u *updater) filterResultTags(results result.List) result.List {
	return results.TransformTags(func(t result.Tags) result.Tags {
		type HighestPrioritySetTag struct {
			tag      string
			priority int
		}
		// Set name to highest priority tag for that set
		best := map[string]HighestPrioritySetTag{}
		for tag := range t {
			sp, ok := u.in.Tags[tag]
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
