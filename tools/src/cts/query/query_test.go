package query_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"github.com/go-test/deep"
)

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
