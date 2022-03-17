// Package set implements a basic generic unordered set using a map
package container_test

import (
	"testing"

	"github.com/google/go-cmp/cmp"
)

func expectEq(t *testing.T, name string, got, expect interface{}) {
	t.Helper()
	if diff := cmp.Diff(got, expect); diff != "" {
		t.Errorf("%v:\n%v", name, diff)
	}
}
