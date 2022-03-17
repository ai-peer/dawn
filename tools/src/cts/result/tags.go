package result

import (
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
)

// The delimiter used to separate tags
const TagDelimiter = ";"

// Tags is a collection of strings used to annotate test results with the test
// configuration.
type Tags = container.Set[string]

// Returns a new tag set with the given tags
func NewTags(tags ...string) Tags {
	return Tags(container.NewSet(tags...))
}

// TagsToString returns the tags sorted and joined using the TagDelimiter
func TagsToString(t Tags) string {
	return strings.Join(t.List(), TagDelimiter)
}
