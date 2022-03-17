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

package result

import (
	"sort"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

// MinimalVariantTags accepts a list of tag-sets (e.g GPU tags, OS tags, etc),
// and returns an optimized list of variants, folding together variants that
// have identical results, and removing redundant tags.
//
// MinimalVariantTags will attempt to remove variant tags starting with the
// first set of tags in tagSets, then second, and so on.
//
// MinimalVariantTags assumes that:
// * There are no duplicate results (same query, same tags) in l.
func (l List) MinimalVariantTags(tagSets []Tags) []Variant {
	// VariantData holds the variant tags and a map of test query to status.
	type VariantData struct {
		tags          Variant
		queryToStatus map[query.Query]Status
	}

	variants := []VariantData{}

	// Build the initial list of variants from List.Variants(), and construct
	// the queryToStatus map for each.
	// TODO: This can be optimized, by iterating over the results and binning.
	for _, tags := range l.Variants() {
		variant := VariantData{
			tags:          Variant(tags),
			queryToStatus: map[query.Query]Status{},
		}
		for _, r := range l.FilterByTags(tags) {
			variant.queryToStatus[r.Query] = r.Status
		}
		variants = append(variants, variant)
	}

	// canReduce checks that the variant would match the same results if the
	// tags were reduced to 'tags'. Returns true if the variant's tags could
	// be reduced, otherwise false.
	canReduce := func(variant VariantData, tags Tags) bool {
		for _, r := range l.FilterByTags(tags) {
			existing, found := variant.queryToStatus[r.Query]
			if !found {
				// Removing the tag has expanded the set of queries.
				return false
			}
			if existing != r.Status {
				// Removing the tag has resulted in two queries with different
				// results.
				return false
			}
		}
		return true
	}

	tryToRemoveTags := func(tags Tags) {
		newVariants := make([]VariantData, 0, len(variants))

		for _, v := range variants {
			// Does the variant even contain these tags?
			if !v.tags.ContainsAny(tags) {
				// Nope. Skip the canReduce() call, and keep the variant.
				newVariants = append(newVariants, v)
				continue
			}

			// Build the new tag set with 'tag' removed.
			newTags := v.tags.Clone()
			newTags.RemoveAll(tags)

			// Check wether removal of these tags affected the outcome.
			if !canReduce(v, newTags) {
				// Removing these tags resulted in differences.
				return // Abort
			}
			newVariants = append(newVariants, VariantData{newTags, v.queryToStatus})
		}

		// Remove variants that are now subsets of others.
		// Start by sorting the variants by number of tags.
		// This ensures that the variants with fewer tags (fewer constraints)
		// come first.
		sort.Slice(newVariants, func(i, j int) bool {
			return len(newVariants[i].tags) < len(newVariants[j].tags)
		})

		// Now check each variant's tags against the previous variant tags.
		// As we've sorted, we know that subsets come after the supersets.
		variants = []VariantData{}
	nextVariant:
		for i, v1 := range newVariants {
			for _, v2 := range newVariants[:i] {
				if v1.tags.ContainsAll(v2.tags) {
					continue nextVariant // v1 is a subset of v2. Omit.
				}
			}
			variants = append(variants, v1)
		}
	}

	for _, tags := range tagSets {
		tryToRemoveTags(tags)
	}

	out := make([]Variant, len(variants))
	for i, v := range variants {
		out[i] = v.tags
	}
	return out
}
