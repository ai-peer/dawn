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
	cfg := expectations.Config{
		Tags: expectations.Tags{
			OS:  []string{"os_a", "os_b"},
			GPU: []string{"gpu_a", "gpu_b"},
		},
	}

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
			err:          "no results provided",
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "missing test results",
			expectations: `crbug.com/a/123 a:missing,test,result:* [ FAIL ]

[ os_a gpu_a ] some:other,test:* [ FAIL ]
[ os_b gpu_b ] some:other,test:* [ FAIL ]
`,
			results: result.List{
				result.Result{
					Query:   Q("some:other,test:*"),
					Variant: result.Variant{OS: "os_a", GPU: "gpu_a"},
					Status:  result.Fail,
				},
				result.Result{
					Query:   Q("some:other,test:*"),
					Variant: result.Variant{OS: "os_b", GPU: "gpu_b"},
					Status:  result.Fail,
				},
			},
			updated: `
[ os_a gpu_a ] some:other,test:* [ FAIL ]
[ os_b gpu_b ] some:other,test:* [ FAIL ]
`,
			messages: []expectations.Diagnostic{
				{1, "'a:missing,test,result:*' was not tested for [OS: 'os_a', GPU: 'gpu_a']"},
				{1, "'a:missing,test,result:*' was not tested for [OS: 'os_b', GPU: 'gpu_b']"},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ os_a gpu_a ] a:b,c:* [ FAIL ]
crbug.com/a/123 [ os_b gpu_b ] a:b,c:* [ ABORT ]
`,
			results: result.List{
				result.Result{
					Query:   Q("a:b,c:*"),
					Variant: result.Variant{OS: "os_a", GPU: "gpu_a"},
					Status:  result.Pass,
				},
				result.Result{
					Query:   Q("a:b,c:*"),
					Variant: result.Variant{OS: "os_b", GPU: "gpu_b"},
					Status:  result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ os_b gpu_b ] a:b,c:* [ ABORT ]
`,
			messages: []expectations.Diagnostic{},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "one expectation now passes",
			expectations: `
crbug.com/a/123 [ os_a gpu_a ] a:b,c:* [ FAIL ]
crbug.com/a/123 [ os_b gpu_b ] a:b,c:* [ ABORT ]
`,
			results: result.List{
				result.Result{
					Query:   Q("a:b,c:*"),
					Variant: result.Variant{OS: "os_a", GPU: "gpu_a"},
					Status:  result.Pass,
				},
				result.Result{
					Query:   Q("a:b,c:*"),
					Variant: result.Variant{OS: "os_b", GPU: "gpu_b"},
					Status:  result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ os_b gpu_b ] a:b,c:* [ ABORT ]
`,
			messages: []expectations.Diagnostic{},
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "new test results",
			expectations: `# A comment`,
			results: result.List{
				result.Result{
					Query:   Q("a:b,c:x*"),
					Variant: result.Variant{OS: "os_a", GPU: "gpu_a"},
					Status:  result.Abort,
				},
				result.Result{
					Query:   Q("a:b,c:y*"),
					Variant: result.Variant{OS: "os_a", GPU: "gpu_a"},
					Status:  result.Skip,
				},
			},
			updated: `# A comment

# New expectations. Please triage:
crbug.com/dawn/TODO [ os_a gpu_a ] a:b,c:x* [ ABORT ]
crbug.com/dawn/TODO [ os_a gpu_a ] a:b,c:y* [ SKIP ]
`,
			messages: []expectations.Diagnostic{},
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

		if diff := deep.Equal(ex.String(), test.updated); diff != nil {
			t.Errorf("'%v': updated was not as expected:\n%v", test.name, diff)
		}
	}
}
