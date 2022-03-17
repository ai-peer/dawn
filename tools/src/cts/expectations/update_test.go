// Copyright 2022 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package expectations_test

import (
	"strings"
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"github.com/google/go-cmp/cmp"
)

var Q = query.Parse

func TestUpdate(t *testing.T) {
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
		diagnostics  []expectations.Diagnostic
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

some:other,test:* [ Failure ]
`,
			results: result.List{
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.NewTags("os-a", "gpu-a"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("some:other,test:*"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Failure,
				},
			},
			updated: `
some:other,test:* [ Failure ]
`,
			diagnostics: []expectations.Diagnostic{
				{
					Severity: expectations.Warning,
					Line:     headerLines + 2,
					Message:  "no results found for 'a:missing,test,result:*'",
				},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "expectation test now passes",
			expectations: `
crbug.com/a/123 [ gpu-a os-a ] a:b,c:* [ Failure ]
crbug.com/a/123 [ gpu-b os-b ] a:b,c:* [ Failure ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-a", "gpu-a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ os-b ] a:b,c:* [ Failure ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "expectation case now passes",
			expectations: `
crbug.com/a/123 [ gpu-a os-a ] a:b,c:d [ Failure ]
crbug.com/a/123 [ gpu-b os-b ] a:b,c:d [ Failure ]
`,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:d"),
					Tags:   result.NewTags("os-a", "gpu-a"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("a:b,c:d"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Abort,
				},
			},
			updated: `
crbug.com/a/123 [ os-b ] a:b,c:d: [ Failure ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "new test results",
			expectations: `# A comment`,
			results: result.List{
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_a:*"),
					Tags:   result.NewTags("os-a", "gpu-a"),
					Status: result.Abort,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_a:*"),
					Tags:   result.NewTags("os-a", "gpu-b"),
					Status: result.Abort,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_c:case=4;*"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Crash,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_c:case=5;*"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b;case=5;*"),
					Tags:   result.NewTags("os-b", "gpu-b"),
					Status: result.Pass,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b:*"),
					Tags:   result.NewTags("os-a", "gpu-a"),
					Status: result.Skip,
				},
				result.Result{
					Query:  Q("suite:dir_a,dir_b:test_b:*"),
					Tags:   result.NewTags("os-b", "gpu-a"),
					Status: result.Pass,
				},
			},
			updated: `# A comment

# New failures. Please triage:
crbug.com/dawn/0000 [ gpu-b os-a ] suite:* [ Failure ]
crbug.com/dawn/0000 [ gpu-a os-a ] suite:dir_a,dir_b:test_a:* [ Failure ]
crbug.com/dawn/0000 [ gpu-a os-a ] suite:dir_a,dir_b:test_b:* [ Skip ]
crbug.com/dawn/0000 [ gpu-b os-b ] suite:dir_a,dir_b:test_c:* [ Failure ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "filter unknown tags",
			expectations: ``,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-a", "gpu-x"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-b", "gpu-x"),
					Status: result.Crash,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-x", "gpu-b"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-x", "gpu-a"),
					Status: result.Crash,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-c", "gpu-c"),
					Status: result.Pass,
				},
			},
			updated: `
# New failures. Please triage:
crbug.com/dawn/0000 [ gpu-a ] a:* [ Failure ]
crbug.com/dawn/0000 [ gpu-b ] a:* [ Failure ]
crbug.com/dawn/0000 [ os-a ] a:* [ Failure ]
crbug.com/dawn/0000 [ os-b ] a:* [ Failure ]
`,
		},
		{ //////////////////////////////////////////////////////////////////////
			name:         "prioritize tag sets",
			expectations: ``,
			results: result.List{
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-a", "os-c", "gpu-b"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("gpu-a", "os-b", "gpu-c"),
					Status: result.Failure,
				},
				result.Result{
					Query:  Q("a:b,c:*"),
					Tags:   result.NewTags("os-c", "gpu-c"),
					Status: result.Pass,
				},
			},
			updated: `
# New failures. Please triage:
crbug.com/dawn/0000 [ gpu-b os-c ] a:* [ Failure ]
crbug.com/dawn/0000 [ gpu-c os-b ] a:* [ Failure ]
`,
		},
	} {
		ex, err := expectations.Parse(header + test.expectations)
		if err != nil {
			t.Fatalf("'%v': expectations.Parse():\n%v", test.name, err)
		}

		errMsg := ""
		diagnostics, err := ex.Update(test.results)
		if err != nil {
			errMsg = err.Error()
		}
		if diff := cmp.Diff(errMsg, test.err); diff != "" {
			t.Errorf("'%v': expectations.Update() error:\n%v", test.name, diff)
		}

		if diff := cmp.Diff(diagnostics, test.diagnostics); diff != "" {
			t.Errorf("'%v': diagnostics were not as expected:\n%v", test.name, diff)
		}

		if diff := cmp.Diff(
			strings.Split(ex.String(), "\n"),
			strings.Split(header+test.updated, "\n")); diff != "" {
			t.Errorf("'%v': updated was not as expected:\n%v", test.name, diff)
		}
	}
}
