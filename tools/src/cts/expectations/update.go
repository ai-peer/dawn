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

type Diagnostic struct {
	Line    int
	Message string
}

// A query tree with no data.
// Used for filtering the full result set to sub-trees.
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

func (qt *queryTree) markAsConsumed(q query.Query, t result.Tags, line int) error {
	l, err := qt.tree.Glob(q)
	if err != nil {
		return err
	}
	for _, r := range l {
		for _, idx := range qt.resultsByQuery[r.Query] {
			if qt.results[idx].Tags.ContainsAll(t) {
				if at := qt.consumedAt[idx]; at > 0 {
					return fmt.Errorf("'%v' already has expectation at line %v", q, at)
				}
				qt.results[idx].Status = consumed
				qt.consumedAt[idx] = line
			}
		}
	}
	return nil
}

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

func (c *Content) Update(results result.List, cfg *Config) ([]Diagnostic, error) {
	// Make a copy of the results. This code mutates the list.
	results = append(result.List{}, results...)

	simplifyStatuses(results)

	tagSets := make([]result.Tags, len(c.TagSets))
	for i, s := range c.TagSets {
		tagSets[len(tagSets)-i-1] = s.Tags
	}

	u := updater{
		in:       *c,
		out:      Content{},
		cfg:      cfg,
		qt:       newQueryTree(results),
		variants: gatherVariants(results, c.Tags),
		tagSets:  tagSets,
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
	tagSets  []result.Tags
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

func (u *updater) expectation(in Expectation, keep bool) ([]Expectation, error) {
	diag := func(msg string, args ...interface{}) {
		u.diags = append(u.diags, Diagnostic{in.Line, fmt.Sprintf(msg, args...)})
	}

	q := query.Parse(in.Query)

	expectSkip := container.NewSet(in.Status...).Contains(string(result.Skip))

	// Grab all the results that match the query
	unfiltered, err := u.qt.resultsFor(q)
	if err != nil {
		if errors.As(err, &query.ErrNoDataForQuery{}) {
			if !expectSkip {
				diag("no results found for '%v'", in.Query)
			}
			if keep {
				return []Expectation{in}, nil
			}
			return []Expectation{}, nil
		}
		return nil, err
	}

	if len(unfiltered) == 0 {
		if !expectSkip {
			diag("no results found for '%v'", in.Query)
		}
		return []Expectation{in}, nil
	}

	// Filter these results to the expectation's tags
	filtered := unfiltered.FilterByTags(in.Tags)

	if len(filtered) == 0 {
		if !expectSkip {
			diag("no results found for '%v' with tags %v", in.Query, in.Tags)
		}
		return []Expectation{in}, nil
	}

	if keep { // Expectation chunk marked with 'KEEP'
		// Add a diagnostic if the expectation is a full-pass
		if s := filtered.Statuses(); len(s) == 1 && s.One() == result.Pass {
			diag("all test for '%v' with tags %v pass", in.Query, in.Tags)
		}

		// Just mark the tests as consumed
		if err := u.qt.markAsConsumed(q, in.Tags, in.Line); err != nil {
			diag("%v", err)
		}
		return []Expectation{in}, nil
	}

	// Build the minimal set of variants (tags) needed to generate the
	// expectations for the query.
	minimalVariants := u.
		filterResultTags(unfiltered).
		MinimalVariantTags(u.tagSets)

	// For each minimized variant...
	results := result.List{}
	for _, variant := range minimalVariants {
		tree := result.StatusTree{}
		for _, r := range filtered {
			if r.Tags.ContainsAll(variant) {
				tree.Add(r.Query, r.Status)
			}
		}

		// Build a tree and reduce it
		tree.ReduceUnder(q, treeReducer)

		for _, qs := range tree.List() {
			results = append(results, result.Result{
				Query:  qs.Query,
				Tags:   variant,
				Status: qs.Data,
			})
		}
	}

	// Filter out any results that passed or have already been consumed
	out := results.Filter(func(r result.Result) bool {
		if r.Status == result.Pass || r.Status == consumed {
			return false
		}
		return true
	})

	for _, r := range results {
		if err := u.qt.markAsConsumed(r.Query, r.Tags, in.Line); err != nil {
			diag("%v", err)
		}
	}

	return u.resultsToExpectations(out, in.Bug, in.Comment), nil
}

func (u *updater) addNewExpectations() error {
	// For each variant...
	resultsByQuery := map[query.Query]result.List{}
	for _, variant := range u.variants {
		// Build a tree and reduce it
		tree, err := u.qt.results.FilterByTags(variant).StatusTree()
		if err != nil {
			return fmt.Errorf("while building tree for tags '%v': %w", variant, err)
		}
		tree.Reduce(treeReducer)
		for _, qd := range tree.List() {
			resultsByQuery[qd.Query] = append(resultsByQuery[qd.Query],
				result.Result{
					Query:  qd.Query,
					Tags:   variant,
					Status: qd.Data,
				})
		}
	}

	// Reduce the tags
	reducedTags, err := u.reduceTags(resultsByQuery)
	if err != nil {
		return err
	}

	// Filter out any results that are either Pass or we've already consumed
	filtered := reducedTags.Filter(func(r result.Result) bool {
		return r.Status != result.Pass && r.Status != consumed
	})

	flakes, failures := result.List{}, result.List{}
	for _, r := range filtered {
		if r.Status == result.RetryOnFailure {
			flakes = append(flakes, r)
		} else {
			failures = append(failures, r)
		}
	}

	for _, set := range []struct {
		results result.List
		comment string
	}{
		{flakes, newFlakesComment},
		{failures, newFailuresComment},
	} {
		newExpectations := u.resultsToExpectations(set.results, "crbug.com/dawn/0000", "")
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
		glob = u.filterResultTags(glob)

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
