package expectations

import (
	"errors"
	"fmt"
	"log"
	"strings"

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

func (c *Content) Update(results result.List, cfg *Config) ([]Diagnostic, error) {
	// Start by simplifying the results
	log.Println("Reducing tags...")
	results.ReduceTags()

	uniqueTags := results.UniqueTags()
	//if c.Tags.Set != nil {
	//	for i := range uniqueTags {
	//		uniqueTags[i].Set = c.Tags.Intersection(uniqueTags[i].Set)
	//
	//		// HACK: These always end up with duplicates in the tag-set, leading to validation errors.
	//		// Fix.
	//		uniqueTags[i].Remove("amd")
	//		uniqueTags[i].Remove("intel")
	//		uniqueTags[i].Remove("mac")
	//		uniqueTags[i].Remove("nvidia")
	//		uniqueTags[i].Remove("win")
	//	}
	//}
	log.Println("Building result tree...")
	qt := newQueryTree(results)

	u := updater{
		in:         *c,
		out:        Content{},
		cfg:        cfg,
		qt:         qt,
		uniqueTags: uniqueTags,
	}

	log.Println("Updating expectations...")
	if err := u.build(); err != nil {
		return nil, err
	}
	*c = u.out
	return u.diags, nil
}

type updater struct {
	in, out    Content
	cfg        *Config
	qt         queryTree
	diags      []Diagnostic
	uniqueTags []result.Tags
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
	newExpectations := []Expectation{}
	for _, tags := range u.uniqueTags {
		t, err := tree.New(u.qt.results.FilterByTags(tags))
		if err != nil {
			return fmt.Errorf("while adding new expectations for tags '%v': %w", tags, err)
		}
		t.Reduce()
		results := t.AllResults().Filter(func(r result.Result) bool {
			return r.Status != result.Pass && r.Status != consumed
		})
		for _, r := range results {
			newExpectations = append(newExpectations, Expectation{
				Bug:    "crbug.com/dawn/0000",
				Tags:   tags,
				Test:   u.cfg.TestPrefix + r.Query.String(),
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
	if !strings.HasPrefix(in.Test, u.cfg.TestPrefix) {
		return []Expectation{in}, nil
	}

	diag := func(msg string, args ...interface{}) {
		u.diags = append(u.diags, Diagnostic{in.Line, fmt.Sprintf(msg, args...)})
	}

	q, err := query.Parse(strings.TrimPrefix(in.Test, u.cfg.TestPrefix))
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

	results.ReduceTags()

	out := []Expectation{}
	for _, tags := range results.UniqueTags() {
		// Build a tree with just the results of the given unique set of tags
		t, err := tree.New(results.FilterByTags(tags))
		if err != nil {
			return nil, err
		}
		// Reduce the results
		t.ReduceTo(q)
		// Collect the reduced results
		reduced := t.AllResults()
		// Filter the results to just those that are non-passing
		reduced = reduced.Filter(func(r result.Result) bool {
			return r.Status != result.Pass
		})
		for _, r := range reduced {
			// Emit the updated expectations
			out = append(out, Expectation{
				Line:   in.Line,
				Bug:    in.Bug,
				Tags:   tags,
				Test:   in.Test,
				Status: []string{string(r.Status)},
			})
		}
	}

	return out, nil
}
