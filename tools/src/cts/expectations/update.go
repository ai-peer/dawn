package expectations

import (
	"errors"
	"fmt"
	"log"

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
	tags := result.TagsToString(r.Tags)
	for _, idx := range qt.resultsByQuery[r.Query] {
		if result.TagsToString(qt.results[idx].Tags) == tags {
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
		for tag := range t {
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
	if c.Tags != nil {
		log.Println("Filtering tags...")
		results = c.filterResultTags(results)
	}

	log.Println("Building result tree...")
	qt := newQueryTree(results)

	u := updater{
		in:       *c,
		out:      Content{},
		cfg:      cfg,
		qt:       qt,
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

	// Grab all the results that match the query
	unfiltered, err := u.qt.resultsFor(q)
	if err != nil {
		if errors.As(err, &tree.ErrNoResultForQuery{}) {
			diag("no tests with query '%v'", in.Test)
			return nil, nil
		}
		return nil, err
	}

	// Filter these results to the expectation's tags
	unreduced := unfiltered.FilterByTags(in.Tags)

	// Mark all the results as consumed
	for _, r := range unreduced {
		u.qt.markAsConsumed(r)
	}

	results := result.List{}

	// For each variant...
	for _, variant := range u.variants {
		// Build a tree and reduce it
		t, err := tree.New(unreduced.FilterByTags(variant))
		if err != nil {
			return nil, fmt.Errorf("while building tree for tags '%v': %w", u.variants, err)
		}
		t.ReduceTo(q)
		results = append(results, t.AllResults(variant)...)
	}

	return u.resultsToExpectations(results, in.Bug), nil
}

func (u *updater) addNewExpectations() error {
	results := result.List{}

	// For each variant...
	for _, variant := range u.variants {
		// Build a tree and reduce it
		t, err := tree.New(u.qt.results.FilterByTags(variant))
		if err != nil {
			return fmt.Errorf("while building tree for tags '%v': %w", u.variants, err)
		}
		t.Reduce()
		results = append(results, t.AllResults(variant)...)
	}

	results.Sort()

	newExpectations := u.resultsToExpectations(results, "crbug.com/dawn/0000")

	if len(newExpectations) > 0 {
		u.out.MaybeAddBlankLine()
		u.out.Chunks = append(u.out.Chunks, Chunk{
			Comments:     []string{"# New expectations. Please triage:"},
			Expectations: newExpectations,
		})
	}

	return nil
}

func (u *updater) resultsToExpectations(results result.List, bug string) []Expectation {
	// Filter out any results that are either Pass or we've already consumed
	results = results.Filter(func(r result.Result) bool {
		return r.Status != result.Pass && r.Status != consumed
	})

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
