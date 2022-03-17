package expectations_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"github.com/go-test/deep"
)

func TestParse(t *testing.T) {
	type Test struct {
		name   string
		in     string
		expect expectations.Content
	}
	for _, test := range []Test{
		{
			name:   "empty",
			in:     ``,
			expect: expectations.Content{},
		}, {
			name: "single line comment",
			in:   `# a comment`,
			expect: expectations.Content{
				{Comments: []string{`# a comment`}},
			},
		}, {
			name: "single line comment, followed by newline",
			in: `# a comment
`,
			expect: expectations.Content{
				{Comments: []string{`# a comment`}},
			},
		}, {
			name: "newline, followed by single line comment",
			in: `
# a comment`,
			expect: expectations.Content{
				{},
				{Comments: []string{`# a comment`}},
			},
		}, {
			name: "comments separated by single newline",
			in: `# comment 1
# comment 2`,
			expect: expectations.Content{
				{
					Comments: []string{
						`# comment 1`,
						`# comment 2`,
					},
				},
			},
		}, {
			name: "comments separated by two newlines",
			in: `# comment 1

# comment 2`,
			expect: expectations.Content{
				{Comments: []string{`# comment 1`}},
				{},
				{Comments: []string{`# comment 2`}},
			},
		}, {
			name: "comments separated by multiple newlines",
			in: `# comment 1



# comment 2`,
			expect: expectations.Content{
				{Comments: []string{`# comment 1`}},
				{},
				{Comments: []string{`# comment 2`}},
			},
		}, {
			name: "test, single result",
			in:   `abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, multiple results",
			in:   `abc,def [ FAIL SLOW ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Test:   "abc,def",
							Status: []string{"FAIL", "SLOW"},
						},
					},
				},
			},
		}, {
			name: "test, with single tag",
			in:   `[ Win ] abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Tags:   result.TagsFrom("Win"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, with multiple tags",
			in:   `[ Win Mac ] abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Tags:   result.TagsFrom("Win", "Mac"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, with bug",
			in:   `crbug.com/123 abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Bug:    "crbug.com/123",
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, with bug and tag",
			in:   `crbug.com/123 [ Win ] abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Expectations: []expectations.Expectation{
						{
							Line:   1,
							Bug:    "crbug.com/123",
							Tags:   result.TagsFrom("Win"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, with comment",
			in: `# a comment
crbug.com/123 [ Win ] abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Comments: []string{`# a comment`},
					Expectations: []expectations.Expectation{
						{
							Line:   2,
							Bug:    "crbug.com/123",
							Tags:   result.TagsFrom("Win"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "test, with multiple comments",
			in: `# comment 1
# comment 2
crbug.com/123 [ Win ] abc,def [ FAIL ]`,
			expect: expectations.Content{
				{
					Comments: []string{`# comment 1`, `# comment 2`},
					Expectations: []expectations.Expectation{
						{
							Line:   3,
							Bug:    "crbug.com/123",
							Tags:   result.TagsFrom("Win"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
			},
		}, {
			name: "comment, test, newline, comment",
			in: `# comment 1
crbug.com/123 abc_def [ Skip ]

### comment 2`,
			expect: expectations.Content{
				{
					Comments: []string{`# comment 1`},
					Expectations: []expectations.Expectation{
						{
							Line:   2,
							Bug:    "crbug.com/123",
							Test:   "abc_def",
							Status: []string{"Skip"},
						},
					},
				},
				{},
				{Comments: []string{`### comment 2`}},
			},
		},
		{
			name: "complex",
			in: `# comment 1

# comment 2
# comment 3

crbug.com/123 [ Win ] abc,def [ FAIL ]

# comment 4
# comment 5
crbug.com/456 [ Mac ] ghi_jkl [ PASS ]
# comment 6

# comment 7
`,
			expect: expectations.Content{
				{Comments: []string{`# comment 1`}},
				{},
				{Comments: []string{`# comment 2`, `# comment 3`}},
				{},
				{
					Expectations: []expectations.Expectation{
						{
							Line:   6,
							Bug:    "crbug.com/123",
							Tags:   result.TagsFrom("Win"),
							Test:   "abc,def",
							Status: []string{"FAIL"},
						},
					},
				},
				{},
				{
					Comments: []string{`# comment 4`, `# comment 5`},
					Expectations: []expectations.Expectation{
						{
							Line:   10,
							Bug:    "crbug.com/456",
							Tags:   result.TagsFrom("Mac"),
							Test:   "ghi_jkl",
							Status: []string{"PASS"},
						},
					},
				},
				{Comments: []string{`# comment 6`}},
				{},
				{Comments: []string{`# comment 7`}},
			},
		},
	} {

		got, err := expectations.Parse("my-expectations.txt", test.in)
		if err != nil {
			t.Fatalf("'%v': Parse() returned %v", test.name, err)
		}
		if diff := deep.Equal(got, test.expect); diff != nil {
			t.Errorf("'%v': Parse() was not as expected:\n%v", test.name, diff)
		}
	}
}
