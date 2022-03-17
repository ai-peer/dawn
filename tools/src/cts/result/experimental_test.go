package result_test

import (
	"fmt"
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"github.com/google/go-cmp/cmp"
)

func EXP_TestReduce(t *testing.T) {
	type Test struct {
		name    string
		results result.List
		expect  result.List
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name:    "empty",
			results: result.List{},
			expect:  result.List{},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "single",
			results: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("x", "y"), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T(), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "full merge",
			results: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("x", "y"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "z"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z"), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T(), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "partial merge (1)",
			results: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("x", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "3"), Status: result.Failure},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "3"), Status: result.Failure},
			},
			expect: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("3"), Status: result.Failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "no merge",
			results: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("x", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "2"), Status: result.Failure},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "3"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "1"), Status: result.Failure},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "3"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "3"), Status: result.Failure},
			},
			expect: result.List{
				{Query: Q("a:b,c:d,*"), Tags: T("x", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "2"), Status: result.Failure},
				{Query: Q("a:b,c:d,*"), Tags: T("x", "3"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "1"), Status: result.Failure},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("y", "3"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "1"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "2"), Status: result.Pass},
				{Query: Q("a:b,c:d,*"), Tags: T("z", "3"), Status: result.Failure},
			},
		},
	} {
		preReduce := fmt.Sprint(test.results)
		got := test.results.EXP_ReduceTags()
		postReduce := fmt.Sprint(test.results)
		if diff := cmp.Diff(got, test.expect); diff != "" {
			t.Errorf("'%v': Reduce() diff:\n%v", test.name, diff)
		}
		if diff := cmp.Diff(preReduce, postReduce); diff != "" {
			t.Errorf("'%v': Reduce() modified original list:\n%v", test.name, diff)
		}
	}
}
