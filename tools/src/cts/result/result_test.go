package result_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"github.com/google/go-cmp/cmp"
)

func Q(name string) query.Query {
	tn, err := query.Parse(name)
	if err != nil {
		panic(err)
	}
	return tn
}

func T(tags ...string) result.Tags {
	return result.NewTags(tags...)
}

func TestStringAndParse(t *testing.T) {
	type Test struct {
		result result.Result
		expect string
	}
	for _, test := range []Test{
		{
			result.Result{
				Query:  Q(`a`),
				Status: result.Failure,
			},
			`a Failure`,
		}, {
			result.Result{
				Query:  Q(`a:b,c,*`),
				Tags:   T("x"),
				Status: result.Pass,
			},
			`a:b,c,* x Pass`,
		},
		{
			result.Result{
				Query:  Q(`a:b,c:d,*`),
				Tags:   T("zzz", "x", "yy"),
				Status: result.Failure,
			},
			`a:b,c:d,* x;yy;zzz Failure`,
		},
	} {
		if diff := cmp.Diff(test.result.String(), test.expect); diff != "" {
			t.Errorf("'%v'.String() was not as expected:\n%v", test.result, diff)
		}
		parsed, err := result.Parse(test.expect)
		if err != nil {
			t.Fatalf("Parse('%v') returned %v", test.expect, err)
		}
		if diff := cmp.Diff(parsed, test.result); diff != "" {
			t.Errorf("Parse('%v') was not as expected:\n%v", test.expect, diff)
		}
	}
}
