package tree

import (
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
)

type ErrNodeDoesNotExist struct {
	Query query.Query
}

func (e ErrNodeDoesNotExist) Error() string {
	return fmt.Sprintf("node '%v' does not exist", e.Query)
}
