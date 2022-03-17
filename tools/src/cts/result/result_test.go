package result_test

import (
	"fmt"
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

func TestWriteAndParse(t *testing.T) {
	type Test struct {
		result result.Result
		expect string
	}
	for _, test := range []Test{
		{
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

func TestReduce(t *testing.T) {
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
		got := test.results.ReduceTags()
		postReduce := fmt.Sprint(test.results)
		if diff := cmp.Diff(got, test.expect); diff != "" {
			t.Errorf("'%v': Reduce() diff:\n%v", test.name, diff)
		}
		if diff := cmp.Diff(preReduce, postReduce); diff != "" {
			t.Errorf("'%v': Reduce() modified original list:\n%v", test.name, diff)
		}
	}
}
