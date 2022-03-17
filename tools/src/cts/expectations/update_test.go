package expectations_test

import (
	"strings"
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
	header := `# BEGIN TAG HEADER
# OS
# tags: [ os-a os-b os-c ]
# GPU
# tags: [ gpu-a gpu-b gpu-c ]
# END TAG HEADER
`
	headerLines := strings.Count(header, "\n")

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
			expectations: `
crbug.com/a/123 a:missing,test,result:* [ Failure ]

[ gpu-a os-a ] some:other,test:* [ Failure ]
[ gpu-b os-b ] some:other,test:* [ Failure ]
`,
			results: result.List{
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.TagsFrom("os-a", "gpu-a"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.TagsFrom("os-b", "gpu-b"),
					Status: result.Failure,
				},
			},
			updated: `
[ gpu-a os-a ] some:other,test:* [ Failure ]
[ gpu-b os-b ] some:other,test:* [ Failure ]
`,
			messages: []expectations.Diagnostic{
				{
					Line:    headerLines + 2,
					Message: "no tests with query 'a:missing,test,result:*'",
				},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation test now passes",
			expectations: `
crbug.com/a/123 [ gpu-a os-a ] a:b,c:* [ Failure ]
crbug.com/a/123 [ gpu-b os-b ] a:b,c:* [ Abort ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-a", "gpu-a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-b", "gpu-b"),
					Status: result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ gpu-b os-b ] a:b,c:* [ Abort ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation case now passes",
			expectations: `
crbug.com/a/123 [ gpu-a os-a ] a:b,c:d [ Failure ]
crbug.com/a/123 [ gpu-b os-b ] a:b,c:d [ Abort ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:d:*"),
					Tags:   result.TagsFrom("os-a", "gpu-a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:d:*"),
					Tags:   result.TagsFrom("os-b", "gpu-b"),
					Status: result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ gpu-b os-b ] a:b,c:d [ Abort ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "new test results",
			expectations: `# A comment`,
			results: result.List{
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_a:*"),
					Tags:   result.TagsFrom("os-a", "gpu-a"),
					Status: result.Abort,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_a:*"),
					Tags:   result.TagsFrom("os-a", "gpu-b"),
					Status: result.Abort,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_a:case=4;*"),
					Tags:   result.TagsFrom("os-b", "gpu-b"),
					Status: result.Crash,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b;case=5;*"),
					Tags:   result.TagsFrom("os-b", "gpu-b"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b:*"),
					Tags:   result.TagsFrom("os-a", "gpu-a"),
					Status: result.Skip,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b:*"),
					Tags:   result.TagsFrom("os-b", "gpu-a"),
					Status: result.Pass,
				},
			},
			updated: `# A comment

# New expectations. Please triage:
crbug.com/dawn/0000 [ gpu-a os-a ] suite:dir_a,dir_b:test_a [ Abort ]
crbug.com/dawn/0000 [ gpu-a os-a ] suite:dir_a,dir_b:test_b [ Skip ]
crbug.com/dawn/0000 [ gpu-b os-a ] suite:* [ Abort ]
crbug.com/dawn/0000 [ gpu-b os-b ] suite:dir_a,dir_b:test_a [ Crash ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "filter unknown tags",
			expectations: ``,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-a", "gpu-x"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-x", "gpu-b"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-c", "gpu-c"),
					Status: result.Pass,
				},
			},
			updated: `
# New expectations. Please triage:
crbug.com/dawn/0000 [ gpu-b ] a:* [ Failure ]
crbug.com/dawn/0000 [ os-a ] a:* [ Failure ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "prioritize tag sets",
			expectations: ``,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-a", "os-c", "gpu-b"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("gpu-a", "os-b", "gpu-c"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.TagsFrom("os-c", "gpu-c"),
					Status: result.Pass,
				},
			},
			updated: `
# New expectations. Please triage:
crbug.com/dawn/0000 [ gpu-b os-c ] a:* [ Failure ]
crbug.com/dawn/0000 [ gpu-c os-b ] a:* [ Failure ]
`,
		},
	} {

		ex, err := expectations.Parse("my-expectations.txt", header+test.expectations)
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

		if diff := cmp.Diff(
			strings.Split(ex.String(), "\n"),
			strings.Split(header+test.updated, "\n")); diff != "" {
			t.Errorf("'%v': updated was not as expected:\n%v", test.name, diff)
		}
	}
}
