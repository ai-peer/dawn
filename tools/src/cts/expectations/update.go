// Copyright 2022 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package expectations

import (
	"errors"
	"fmt"
	"log"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

const (
	// Status used to mark results that have been already handled by an
	// expectation.
	consumed result.Status = "<<consumed>>"
	// Chunk comment for new flakes
	newFlakesComment = "# New flakes. Please triage:"
	// Chunk comment for new failures
	newFailuresComment = "# New failures. Please triage:"
)

// queryTree holds tree of queries to all results (no filtering by tag or
// status). The queryTree is used to glob all the results that match a
// particular query.
type queryTree struct {
	results        result.List // All results
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
		return nil, fmt.Errorf("while gathering results for query '%v': %w", q, err)
	}

	out := result.List{}
	for _, r := range l {
		for _, idx := range qt.resultsByQuery[r.Query] {
			out = append(out, qt.results[idx])
		}
	}

	return out, nil
}

func (qt *queryTree) markAsConsumed(q query.Query, t result.Tags, line int) error {
	l, err := qt.tree.Glob(q)
	if err != nil {
		return err
	}
	for _, r := range l {
		for _, idx := range qt.resultsByQuery[r.Query] {
			r := &qt.results[idx]
			if r.Tags.ContainsAll(t) {
				if at := qt.consumedAt[idx]; at > 0 {
					return fmt.Errorf("%v %v already has expectation at line %v", t, q, at)
				}
				r.Status = consumed
				qt.consumedAt[idx] = line
			}
		}
	}
	return nil
}

func (c *Content) Update(results result.List) ([]Diagnostic, error) {
	// Make a copy of the results. This code mutates the list.
	results = append(result.List{}, results...)

	// Replace statuses that the CTS runner doesn't recognize with 'Failure'
	simplifyStatuses(results)

	// Produce a list of tag sets.
	// We reverse the declared order, as webgpu-cts/expectations.txt lists the
	// most important first (OS, GPU, etc), and result.MinimalVariantTags will
	// prioritize folding away the earlier tag-sets.
	tagSets := make([]result.Tags, len(c.Tags.Sets))
	for i, s := range c.Tags.Sets {
		tagSets[len(tagSets)-i-1] = s.Tags
	}

	// Update those expectations!
	log.Println("Updating expectations...")
	u := updater{
		in:      *c,
		out:     Content{},
		qt:      newQueryTree(results),
		tagSets: tagSets,
	}
	if err := u.build(); err != nil {
		return nil, fmt.Errorf("while updating expectations: %w", err)
	}

	*c = u.out
	return u.diags, nil
}

// simplifyStatuses replaces all result statuses that are not 'Pass',
// 'RetryOnFailure', 'Skip' with 'Failure'.
func simplifyStatuses(results result.List) {
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
}

type updater struct {
	in, out Content
	qt      queryTree
	diags   []Diagnostic
	tagSets []result.Tags
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

	// Skip over any untriaged failures / flakes
	// We'll just rebuild them at the end
	if len(in.Comments) > 0 {
		switch in.Comments[0] {
		case newFailuresComment, newFlakesComment:
			return Chunk{}, nil
		}
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

	sort.Slice(out.Expectations, func(i, j int) bool {
		switch {
		case out.Expectations[i].Query < out.Expectations[j].Query:
			return true
		case out.Expectations[i].Query > out.Expectations[j].Query:
			return false
		}
		a := result.TagsToString(out.Expectations[i].Tags)
		b := result.TagsToString(out.Expectations[j].Tags)
		switch {
		case a < b:
			return true
		case a > b:
			return false
		}
		return false
	})

	return out, nil
}

// diag appends a new diagnostic to u.diags with the given severity, line and
// message.
func (u *updater) diag(severity Severity, line int, msg string, args ...interface{}) {
	u.diags = append(u.diags, Diagnostic{
		Severity: severity,
		Line:     line,
		Message:  fmt.Sprintf(msg, args...),
	})
}

func (u *updater) expectation(in Expectation, keep bool) ([]Expectation, error) {
	// noResults is a helper for returning in the situation that the expectation
	// has no test results. If the expectation did not have a 'Skip' result,
	// then a diagnostic will be raised. If the expectation chunk was marked
	// with 'KEEP' then the expectation will be preserved, otherwise stripped
	// from the updated expectations file.
	noResults := func(filteredByTags bool) ([]Expectation, error) {
		if !container.NewSet(in.Status...).Contains(string(result.Skip)) {
			// Expectation does not have a 'Skip' result.
			if filteredByTags {
				u.diag(Warning, in.Line, "no results found for '%v' with tags %v", in.Query, in.Tags)
			} else {
				u.diag(Warning, in.Line, "no results found for '%v'", in.Query)
			}
		}
		if keep {
			// Preserve the expectation
			return []Expectation{in}, nil
		}
		// Remove the no-results expectation
		return []Expectation{}, nil
	}

	// Grab all the results that match the query
	q := query.Parse(in.Query)
	unfiltered, err := u.qt.resultsFor(q)
	switch {
	case errors.As(err, &query.ErrNoDataForQuery{}):
		return noResults(false)
	case err != nil:
		return nil, err
	case len(unfiltered) == 0:
		return noResults(false)
	}

	// Filter these results to the expectation's tags
	filtered := unfiltered.FilterByTags(in.Tags)
	if len(filtered) == 0 {
		return noResults(true)
	}

	if keep { // Was the Expectation chunk was marked with 'KEEP'?
		// Add a diagnostic if all tests of the expectation were 'Pass'
		if s := filtered.Statuses(); len(s) == 1 && s.One() == result.Pass {
			u.diag(Note, in.Line, "all test for '%v' with tags %v pass", in.Query, in.Tags)
		}

		// Mark the tests as consumed
		if err := u.qt.markAsConsumed(q, in.Tags, in.Line); err != nil {
			u.diag(Error, in.Line, "%v", err)
			// Despite the chunk being marked with 'KEEP', this expectation will
			// cause the CTS runner to vomit. Remove it.
			return []Expectation{}, nil
		}
		return []Expectation{in}, nil
	}

	return u.expectationsForRoot(q, unfiltered, in.Line, in.Bug, in.Comment), nil
}

func (u *updater) addNewExpectations() error {
	// Build a list of query roots, where expectations can be seeded.
	seen := container.NewSet[string]()
	roots := []query.Query{}
	for _, variant := range u.qt.results.Variants() {
		// Build a tree and reduce it
		tree, err := u.qt.results.FilterByTags(variant).StatusTree()
		if err != nil {
			return fmt.Errorf("while building tree for tags '%v': %w", variant, err)
		}
		tree.Reduce(treeReducer)
		for _, qd := range tree.List() {
			key := qd.Query.String()
			if !seen.Contains(key) {
				seen.Add(key)
				roots = append(roots, qd.Query)
			}
		}
	}

	// Sort the roots
	sort.Slice(roots, func(i, j int) bool {
		return roots[i].Compare(roots[j]) <= 0
	})

	expectations := []Expectation{}
	for _, root := range roots {
		results, err := u.qt.resultsFor(root)
		if err != nil {
			return err
		}
		expectations = append(expectations, u.expectationsForRoot(
			root,                  // Root query
			results,               // All results under root
			0,                     // Line number
			"crbug.com/dawn/0000", // Bug
			"",                    // Comment
		)...)
	}

	flakes, failures := []Expectation{}, []Expectation{}
	for _, r := range expectations {
		if container.NewSet(r.Status...).Contains(string(result.RetryOnFailure)) {
			flakes = append(flakes, r)
		} else {
			failures = append(failures, r)
		}
	}

	for _, group := range []struct {
		results []Expectation
		comment string
	}{
		{flakes, newFlakesComment},
		{failures, newFailuresComment},
	} {
		if len(group.results) > 0 {
			u.out.MaybeAddBlankLine()
			u.out.Chunks = append(u.out.Chunks, Chunk{
				Comments:     []string{group.comment},
				Expectations: group.results,
			})
		}
	}

	return nil
}

func (u *updater) expectationsForRoot(
	root query.Query,
	results result.List,
	line int,
	bug string,
	comment string,
) []Expectation {
	// Using the full list of unfiltered tests, generate the minimal set of
	// variants (tags) that uniquely classify the results with differing status.
	minimalVariants := u.
		cleanupTags(results).
		MinimalVariantTags(u.tagSets)

	// For each minimized variant...
	reduced := result.List{}
	for _, variant := range minimalVariants {
		// Build a query tree from this variant...
		tree := result.StatusTree{}
		filtered := results.FilterByTags(variant)
		for _, r := range filtered {
			// Note: variants may overlap, but overlaped queries will have
			// identical statuses, so we can just ignore the error for Add().
			tree.Add(r.Query, r.Status)
		}

		// ... and reduce the tree by collapsing sub-trees that have common
		// statuses.
		tree.ReduceUnder(root, treeReducer)

		// Append the reduced tree nodes to the results list
		for _, qs := range tree.List() {
			reduced = append(reduced, result.Result{
				Query:  qs.Query,
				Tags:   variant,
				Status: qs.Data,
			})
		}
	}

	// Filter out any results that passed or have already been consumed
	filtered := reduced.Filter(func(r result.Result) bool {
		return r.Status != result.Pass && r.Status != consumed
	})

	// Mark all the new expectation results as consumed.
	for _, r := range filtered {
		if err := u.qt.markAsConsumed(r.Query, r.Tags, line); err != nil {
			u.diag(Error, line, "%v", err)
		}
	}

	return u.resultsToExpectations(filtered, bug, comment)
}

func (u *updater) reduceTags(resultsByQuery map[query.Query]result.List) (result.List, error) {
	out := result.List{}
	for q, l := range resultsByQuery {
		// Obtain the full list of results for the given query.
		// We need this to safely reduce the tag sets.
		glob, err := u.qt.resultsFor(q)
		if err != nil {
			return nil, fmt.Errorf("unexpected error: %w", err)
		}

		// Remove unknown tags, and multiple tags from the same tag sets.
		glob = u.cleanupTags(glob)

		for _, tags := range glob.MinimalVariantTags(u.tagSets) {
			for _, r := range l {
				if r.Tags.ContainsAll(tags) {
					out = append(out, result.Result{
						Query:  q,
						Tags:   tags,
						Status: r.Status,
					})
					break
				}
			}
		}
	}
	out.Sort()
	return out, nil
}

func (u *updater) resultsToExpectations(results result.List, bug, comment string) []Expectation {
	results.Sort()

	out := make([]Expectation, len(results))
	for i, r := range results {
		q := r.Query.String()
		if r.Query.Target() == query.Tests && !r.Query.IsWildcard() {
			// The expectation validator wants a trailing ':' for test queries
			q += query.TargetDelimiter
		}
		out[i] = Expectation{
			Bug:     bug,
			Tags:    r.Tags,
			Query:   q,
			Status:  []string{string(r.Status)},
			Comment: comment,
		}
	}

	return out
}

// Returns results with all tags not found in the expectations list removed,
// and stripping all but the highest priority tag from each tag set.
// The tag sets are defined by the the `BEGIN TAG HEADER` / `END TAG HEADER`
// section at the top of the expectations file.
func (u *updater) cleanupTags(results result.List) result.List {
	return results.TransformTags(func(t result.Tags) result.Tags {
		type HighestPrioritySetTag struct {
			tag      string
			priority int
		}
		// Set name to highest priority tag for that set
		best := map[string]HighestPrioritySetTag{}
		for tag := range t {
			sp, ok := u.in.Tags.ByName[tag]
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

// treeReducer is a function that can be used by StatusTree.Reduce() to reduce
// tree nodes with the same status
func treeReducer(statuses []result.Status) *result.Status {
	counts := map[result.Status]int{}
	for _, s := range statuses {
		counts[s] = counts[s] + 1
	}
	if len(counts) == 1 {
		return &statuses[0] // All the same status
	}
	if counts[consumed] > 0 {
		return nil // Partially consumed trees cannot be merged
	}
	highestNonPassCount := 0
	highestNonPassStatus := result.Failure
	for s, n := range counts {
		if s != result.Pass {
			if percent := (100 * n) / len(statuses); percent > 75 {
				// Over 75% of all the children are of non-pass status s.
				return &s
			}
			if n > highestNonPassCount {
				highestNonPassCount = n
				highestNonPassStatus = s
			}
		}
	}

	if highestNonPassCount > 25 {
		// Over 25 child node failed.
		return &highestNonPassStatus
	}

	return nil
}
