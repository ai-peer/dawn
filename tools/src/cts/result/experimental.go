package result

import (
	"sort"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

// MinimalVariantTags returns the reduced list by combining results that have redundant tags.
func (l List) MinimalVariantTags(tagSets []Tags) []Tags {
	type Variant struct {
		tags          Tags
		queryToStatus map[query.Query]Status
	}

	// Build the set of variants from the input list
	variants := []Variant{}
	for _, tags := range l.UniqueTags() {
		variant := Variant{
			tags:          tags,
			queryToStatus: map[query.Query]Status{},
		}
		for _, r := range l.FilterByTags(tags) {
			variant.queryToStatus[r.Query] = r.Status
		}
		variants = append(variants, variant)
	}

	canReduce := func(variant Variant, tags Tags) bool {
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
		newVariants := make([]Variant, 0, len(variants))

		for _, variant := range variants {
			// Does the variant even contain these tags?
			if !variant.tags.ContainsAny(tags) {
				// Nope. Skip the canReduce() call, and keep the variant.
				newVariants = append(newVariants, variant)
				continue
			}

			// Build the new tag set with 'tag' removed.
			newTags := variant.tags.Clone()
			newTags.RemoveAll(tags)

			// Check wether removal of these tags affected the outcome.
			if !canReduce(variant, newTags) {
				// Removing these tags resulted in differences.
				return // Abort
			}
			newVariants = append(newVariants, Variant{newTags, variant.queryToStatus})
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
		variants = []Variant{}
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

	out := make([]Tags, len(variants))
	for i, v := range variants {
		out[i] = v.tags
	}
	return out
}
