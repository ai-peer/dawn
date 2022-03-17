package expectations_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
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
			expectations: `crbug.com/a/123 a:missing,test,result:* [ Fail ]

[ gpu_a os_a ] some:other,test:* [ Fail ]
[ gpu_b os_b ] some:other,test:* [ Fail ]
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
			updated: `[ gpu_a os_a ] some:other,test:* [ Fail ]
[ gpu_b os_b ] some:other,test:* [ Fail ]
`,
			messages: []expectations.Diagnostic{
				{1, "no tests with query 'a:missing,test,result:*'"},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ gpu_a os_a ] a:b,c:* [ Fail ]
crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ Abort ]
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
			updated: `crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ Abort ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ gpu_a os_a ] a:b,c:* [ Fail ]
crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ Abort ]
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
			updated: `crbug.com/a/123 [ gpu_b os_b ] a:b,c:* [ Abort ]
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
crbug.com/dawn/TODO [ gpu_a os_a ] a:b,c:x* [ Abort ]
crbug.com/dawn/TODO [ gpu_a os_a ] a:b,c:y* [ Skip ]
`,
		},
	} {

		ex, err := expectations.Parse("my-expectations.txt", test.expectations)
		if err != nil {
			t.Fatalf("'%v': expectations.Parse():\n%v", test.name, err)
		}

		errMsg := ""
		messages, err := ex.Update(test.results, &cfg)
		if err != nil {
			errMsg = err.Error()
		}
		if diff := cmp.Diff(errMsg, test.err); diff != "" {
			t.Errorf("'%v': expectations.Update():\n%v", test.name, diff)
		}

		if diff := cmp.Diff(messages, test.messages); diff != "" {
			t.Errorf("'%v': messages were not as expected:\n%v", test.name, diff)
		}

		if got := ex.String(); got != test.updated {
			t.Errorf(`'%v': updated was not as expected:
Got:      '%v'
Expected: '%v'`, test.name, got, test.updated)
		}
	}
}
