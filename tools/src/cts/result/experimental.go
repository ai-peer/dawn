package result

import (
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

// ReduceTags returns the reduced list by combining results that have redundant tags.
func (l List) EXP_ReduceTags() List {
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
	out = out.ReplaceDuplicates(func(s Statuses) Status {
		for status := range s {
			return status
		}
		return Unknown // unreachable
	})
	return out
}
