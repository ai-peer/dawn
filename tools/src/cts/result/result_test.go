package result_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"github.com/go-test/deep"
)

func Q(name string) query.Query {
	tn, err := query.Parse(name)
	if err != nil {
		panic(err)
	}
	return tn
}

func TestWriteAndParse(t *testing.T) {
	type Test struct {
		result result.Result
		expect string
	}
	for _, test := range []Test{
		{
			result.Result{
				Query:  Q(`a:b,c,*`),
				Tags:   result.TagsFrom("x"),
				Status: result.Pass,
			},
			`a:b,c,* x PASS`,
		},
		{
			result.Result{
				Query:  Q(`a:b,c:d,*`),
				Tags:   result.TagsFrom("zzz", "x", "yy"),
				Status: result.Fail,
			},
			`a:b,c:d,* x;yy;zzz FAIL`,
		},
	} {
		if diff := deep.Equal(test.result.String(), test.expect); diff != nil {
			t.Errorf("'%v'.String() was not as expected:\n%v", test.result, diff)
		}
		parsed, err := result.Parse(test.expect)
		if err != nil {
			t.Fatalf("Parse('%v') returned %v", test.expect, err)
		}
		if diff := deep.Equal(parsed, test.result); diff != nil {
			t.Errorf("Parse('%v') was not as expected:\n%v", test.expect, diff)
		}
	}
}
