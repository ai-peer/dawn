package expectations_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
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

func TestUpdate(t *testing.T) {
	cfg := expectations.Config{}

	type Test struct {
		name         string
		expectations string
		results      result.List
		updated      string
		messages     []expectations.Diagnostic
		err          string
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name:         "empty results",
			expectations: ``,
			results:      result.List{},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "missing test results",
			expectations: `crbug.com/a/123 a:missing,test,result:* [ FAIL ]

[ gpu_a os_a ] some:other,test:* [ FAIL ]
[ gpu_b os_b ] some:other,test:* [ FAIL ]
`,
			results: result.List{
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.TagsFrom("os_a", "gpu_a"),
					Status: result.Fail,
				},
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.TagsFrom("os_b", "gpu_b"),
					Status: result.Fail,
				},
			},
			updated: `[ gpu_a os_a ] some:other,test:* [ FAIL ]
[ gpu_b os_b ] some:other,test:* [ FAIL ]
`,
			messages: []expectations.Diagnostic{
				{1, "no tests with query 'a:missing,test,result:*'"},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ gpu_a os_a ] a:b,c:* [ FAIL ]
crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ ABORT ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os_a", "gpu_a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os_b", "gpu_b"),
					Status: result.Abort,
				},
			},
			updated: `crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ ABORT ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ gpu_a os_a ] a:b,c:* [ FAIL ]
crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ ABORT ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os_a", "gpu_a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os_b", "gpu_b"),
					Status: result.Abort,
				},
			},
			updated: `crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ ABORT ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "new test results",
			expectations: `# A comment`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:x*"),
					Tags:   result.TagsFrom("os_a", "gpu_a"),
					Status: result.Abort,
				},
				result.Result{
					Query:  Q("a:b,c:y*"),
					Tags:   result.TagsFrom("os_a", "gpu_a"),
					Status: result.Skip,
				},
			},
			updated: `# A comment

# New expectations. Please triage:
crbug.com/dawn/TODO [ gpu_a os_a ] a:b,c:x* [ ABORT ]
crbug.com/dawn/TODO [ gpu_a os_a ] a:b,c:y* [ SKIP ]
`,
		},
	} {

		ex, err := expectations.Parse("my-expectations.txt", test.expectations)
		if err != nil {
			t.Fatalf("'%v': expectations.Parse() returned %v", test.name, err)
		}

		errMsg := ""
		messages, err := ex.Update(test.results, &cfg)
		if err != nil {
			errMsg = err.Error()
		}
		if diff := deep.Equal(errMsg, test.err); diff != nil {
			t.Errorf("'%v': expectations.Update() returned %v", test.name, diff)
		}

		if diff := deep.Equal(messages, test.messages); diff != nil {
			t.Errorf("'%v': messages were not as expected: %v", test.name, diff)
		}

		if got := ex.String(); got != test.updated {
			t.Errorf(`'%v': updated was not as expected:
Got:      '%v'
Expected: '%v'`, test.name, got, test.updated)
		}
	}
}
