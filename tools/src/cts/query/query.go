package query

import (
	"fmt"
	"regexp"
	"strings"
)

type ErrInvalidQuery struct {
	Name string
}

func (e ErrInvalidQuery) Error() string {
	return fmt.Sprintf("invalid test name '%v'", e.Name)
}

// Target is the target of a query, either a Suite, File, Test or Case.
type Target int

// suite:*
// suite:file,*
// suite:file,file,*
// suite:file,file,file:test:*
// suite:file,file,file:test,test:case;*
const (
	// The query targets a suite
	Suite Target = iota
	// The query targets one or more files
	Files
	// The query targets one or more tests
	Tests
	// The query targets one or more test cases
	Cases
)

// Format writes the Target to the fmt.State
func (l Target) Format(f fmt.State, verb rune) {
	switch l {
	case Files:
		fmt.Fprint(f, "files")
	case Tests:
		fmt.Fprint(f, "tests")
	case Cases:
		fmt.Fprint(f, "cases")
	default:
		fmt.Fprint(f, "<invalid>")
	}
}

const (
	TargetDelimiter = ":"
	FileDelimiter   = ","
	TestDelimiter   = ","
	CaseDelimiter   = ";"
)

type Query struct {
	Suite string
	Files string
	Tests string
	Cases string
}

type CaseParameters map[string]string

func (q Query) AppendFiles(f ...string) Query {
	if q.Files == "" {
		q.Files = strings.Join(f, FileDelimiter)
	} else {
		q.Files = q.Files + FileDelimiter + strings.Join(f, FileDelimiter)
	}
	return q
}

func (q Query) SplitFiles() []string {
	if q.Files != "" {
		return strings.Split(q.Files, FileDelimiter)
	}
	return nil
}

func (q Query) AppendTests(t ...string) Query {
	if q.Tests == "" {
		q.Tests = strings.Join(t, TestDelimiter)
	} else {
		q.Tests = q.Tests + TestDelimiter + strings.Join(t, TestDelimiter)
	}
	return q
}

func (q Query) SplitTests() []string {
	if q.Tests != "" {
		return strings.Split(q.Tests, TestDelimiter)
	}
	return nil
}

func (q Query) AppendCases(c ...string) Query {
	if q.Cases == "" {
		q.Cases = strings.Join(c, CaseDelimiter)
	} else {
		q.Cases = q.Cases + CaseDelimiter + strings.Join(c, TestDelimiter)
	}
	return q
}

func (q Query) SplitCases() CaseParameters {
	if q.Cases != "" {
		out := CaseParameters{}
		for _, c := range strings.Split(q.Cases, CaseDelimiter) {
			kv := strings.Split(c, "=")
			if len(kv) == 2 {
				out[kv[0]] = kv[1]
			}
		}
		return out
	}
	return nil
}

func (q Query) Append(t Target, n ...string) Query {
	switch t {
	case Files:
		return q.AppendFiles(n...)
	case Tests:
		return q.AppendTests(n...)
	case Cases:
		return q.AppendCases(n...)
	}
	panic("invalid target")
}

func (q Query) Target() Target {
	if q.Tests != "" {
		if q.Cases != "" {
			return Cases
		}
		return Tests
	}
	return Files
}

func (q Query) IsWildcard() bool {
	switch q.Target() {
	case Suite:
		return q.Suite == "*"
	case Files:
		return strings.HasSuffix(q.Files, "*")
	case Tests:
		return strings.HasSuffix(q.Tests, "*")
	case Cases:
		return strings.HasSuffix(q.Cases, "*")
	}
	return false
}

func (q Query) String() string {
	sb := strings.Builder{}
	sb.WriteString(q.Suite)
	sb.WriteString(TargetDelimiter)
	sb.WriteString(q.Files)
	if q.Tests != "" {
		sb.WriteString(TargetDelimiter)
		sb.WriteString(q.Tests)
		if q.Cases != "" {
			sb.WriteString(TargetDelimiter)
			sb.WriteString(q.Cases)
		}
	}
	return sb.String()
}

func (q Query) Compare(o Query) int {
	for _, cmp := range []struct{ a, b string }{
		{q.Suite, o.Suite},
		{q.Files, o.Files},
		{q.Tests, o.Tests},
		{q.Cases, o.Cases},
	} {
		if cmp.a < cmp.b {
			return -1
		}
		if cmp.a > cmp.b {
			return 1
		}
	}

	return 0
}

func (q Query) Contains(o Query) bool {
	if q.Suite != o.Suite {
		return false
	}
	{
		a, b := q.SplitFiles(), o.SplitFiles()
		for i, f := range a {
			if f == "*" {
				return true
			}
			if i >= len(b) || b[i] != f {
				return false
			}
		}
		if len(a) < len(b) {
			return false
		}
	}
	{
		a, b := q.SplitTests(), o.SplitTests()
		for i, f := range a {
			if f == "*" {
				return true
			}
			if i >= len(b) || b[i] != f {
				return false
			}
		}
		if len(a) < len(b) {
			return false
		}
	}
	{
		a, b := q.SplitCases(), o.SplitCases()
		for key, av := range a {
			if bv, found := b[key]; found && av != bv {
				return false
			}
		}
	}
	return true
}

type WalkCallback func(Query, Target, string) error

func (q Query) Walk(f WalkCallback) error {
	p := Query{Suite: q.Suite}

	if err := f(p, Suite, q.Suite); err != nil {
		return err
	}

	for _, file := range q.SplitFiles() {
		p = p.AppendFiles(file)
		if err := f(p, Files, file); err != nil {
			return err
		}
	}

	for _, test := range q.SplitTests() {
		p = p.AppendTests(test)
		if err := f(p, Tests, test); err != nil {
			return err
		}
	}

	if q.Cases != "" {
		if err := f(q, Cases, q.Cases); err != nil {
			return err
		}
	}

	return nil
}

var reQuery = regexp.MustCompile(`^(\w*):([^:]*)(?::([^:]*)(?::(.*))?)?$`)

func Parse(s string) (Query, error) {
	match := reQuery.FindStringSubmatch(s)
	if len(match) < 5 || match[1] == "" {
		return Query{}, ErrInvalidQuery{s}
	}
	tn := Query{
		Suite: match[1],
		Files: match[2],
		Tests: match[3],
		Cases: match[4],
	}
	return tn, nil
}
