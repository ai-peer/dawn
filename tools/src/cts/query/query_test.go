package query_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"github.com/go-test/deep"
)

func Q(name string) query.Query {
	tn, err := query.Parse(name)
	if err != nil {
		panic(err)
	}
	return tn
}

func TestParse(t *testing.T) {
	type Test struct {
		in     string
		expect query.Query
	}

	for _, test := range []Test{
		{
			"a:*",
			query.Query{
				Suite: "a",
				Files: "*",
			},
		}, {
			"a:b,*",
			query.Query{
				Suite: "a",
				Files: "b,*",
			},
		}, {
			"a:b:*",
			query.Query{
				Suite: "a",
				Files: "b",
				Tests: "*",
			},
		}, {
			"a:b,c:*",
			query.Query{
				Suite: "a",
				Files: "b,c",
				Tests: "*",
			},
		}, {
			"a:b,c:d,*",
			query.Query{
				Suite: "a",
				Files: "b,c",
				Tests: "d,*",
			},
		}, {
			"a:b,c:d,e,*",
			query.Query{
				Suite: "a",
				Files: "b,c",
				Tests: "d,e,*",
			},
		}, {
			"a:b,c:d,e:*",
			query.Query{
				Suite: "a",
				Files: "b,c",
				Tests: "d,e",
				Cases: "*",
			},
		}, {
			"a:b,c:d,e:f=g;h=i;*",
			query.Query{
				Suite: "a",
				Files: "b,c",
				Tests: "d,e",
				Cases: "f=g;h=i;*",
			},
		},
	} {
		got, err := query.Parse(test.in)
		if err != nil {
			t.Errorf("query.Parse('%v') returned %v", test.in, err)
		}
		if diff := deep.Equal(test.expect.Suite, got.Suite); diff != nil {
			t.Errorf("query.Parse('%v').Suite\nexpect: %+v\ngot:    %+v", test.in, test.expect.Suite, got.Suite)
		}
		if diff := deep.Equal(test.expect.Files, got.Files); diff != nil {
			t.Errorf("query.Parse('%v').Files\nexpect: %+v\ngot:    %+v", test.in, test.expect.Files, got.Files)
		}
		if diff := deep.Equal(test.expect.Tests, got.Tests); diff != nil {
			t.Errorf("query.Parse('%v').Tests\nexpect: %+v\ngot:    %+v", test.in, test.expect.Tests, got.Tests)
		}
		if diff := deep.Equal(test.expect.Cases, got.Cases); diff != nil {
			t.Errorf("query.Parse('%v').Cases\nexpect: %+v\ngot:    %+v", test.in, test.expect.Cases, got.Cases)
		}
	}
}

func TestContains(t *testing.T) {
	type Test struct {
		a, b   query.Query
		expect bool
	}

	for _, test := range []Test{
		{Q("a:*"), Q("a:*"), true},
		{Q("a:*"), Q("b:*"), false},
		{Q("a:*"), Q("a:b,*"), true},
		{Q("a:b,*"), Q("a:b,*"), true},
		{Q("a:b,*"), Q("a:c,*"), false},
		{Q("a:b,c,*"), Q("a:b,*"), false},
		{Q("a:b,c,*"), Q("a:b,c,*"), true},
		{Q("a:b,c,*"), Q("a:b,c,d,*"), true},
		{Q("a:b,c,*"), Q("a:b,c:d,*"), true},
		{Q("a:b,c:*"), Q("a:b,c,d,*"), false},
		{Q("a:b,c:d,*"), Q("a:b,c:d,*"), true},
		{Q("a:b,c:d,*"), Q("a:b,c:d,e,*"), true},
		{Q("a:b,c:d,e,*"), Q("a:b,c:d,e,*"), true},
		{Q("a:b,c:d,e,*"), Q("a:b,c:e,f,*"), false},
		{Q("a:b,c:d,e,*"), Q("a:b,c:d,*"), false},
		{Q("a:b:c:d;*"), Q("a:b:c:d;*"), true},
		{Q("a:b:c:d;*"), Q("a:b:c:d;e=1;*"), true},
		{Q("a:b:c:d;*"), Q("a:b:c:d;e=1;*"), true},
		{Q("a:b:c:d;*"), Q("a:b:c:d;e=1;f=2;*"), true},
		{Q("a:b:c:d;e=1;*"), Q("a:b:c:d;*"), true},
		{Q("a:b:c:d;e=1;*"), Q("a:b:c:d;*"), true},
		{Q("a:b:c:d;e=1;f=2;*"), Q("a:b:c:d;*"), true},
		{Q("a:b:c:d;e=1;*"), Q("a:b:c:d;e=2;*"), false},
		{Q("a:b:c:d;e=2;*"), Q("a:b:c:d;e=1;*"), false},
	} {
		if got := test.a.Contains(test.b); got != test.expect {
			t.Errorf("('%v').Contains('%v')\nexpect: %+v\ngot:    %+v", test.a, test.b, test.expect, got)
		}
	}
}
