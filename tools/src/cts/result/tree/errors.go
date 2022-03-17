package tree

import (
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

type ErrNoResultForQuery struct {
	Query query.Query
}

func (e ErrNoResultForQuery) Error() string {
	return fmt.Sprintf("no result for query '%v'", e.Query)
}

type ErrDuplicateResult struct {
	Query query.Query
}

func (e ErrDuplicateResult) Error() string {
	return fmt.Sprintf("duplicate result '%v'", e.Query)
}
