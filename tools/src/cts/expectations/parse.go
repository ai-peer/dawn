package expectations

import (
	"fmt"
	"strings"
)

// Parse parses an expectations file content with the given path
func Parse(path, content string) (*Expectations, error) {
	type LineType int

	const (
		comment LineType = iota
		expectation
		blank
	)

	classifyLine := func(line string) LineType {
		line = strings.TrimSpace(line)
		switch {
		case line == "":
			return blank
		case strings.HasPrefix(line, "#"):
			return comment
		default:
			return expectation
		}
	}

	chunks := []Chunk{}
	var pending Chunk
	flush := func() {
		chunks = append(chunks, pending)
		pending = Chunk{}
	}

	lastLineType := blank
	for i, l := range strings.Split(content, "\n") {
		line := i + 1
		lineType := classifyLine(l)

		if i > 0 {
			switch {
			case
				lastLineType == blank && lineType != blank,             // blank -> !blank
				lastLineType != blank && lineType == blank,             // !blank -> blank
				lastLineType == expectation && lineType != expectation: // expectation -> comment
				flush()
			}
		}

		lastLineType = lineType

		switch lineType {
		case blank:
			continue
		case comment:
			pending.Comments = append(pending.Comments, l)
			continue
		}

		tokens := strings.Split(l, " ")
		column := 1

		syntaxErr := func(msg string) error {
			return fmt.Errorf("%v:%v:%v %v", path, line, column, msg)
		}
		peek := func() string {
			for ; len(tokens) > 0; tokens = tokens[1:] {
				if t := strings.TrimSpace(tokens[0]); t != "" {
					return t
				}
			}
			return ""
		}
		next := func() string {
			t := peek()
			if t == "" {
				return ""
			}
			column += len(tokens[0])
			tokens = tokens[1:]
			return t
		}
		tags := func() ([]string, error) {
			if peek() != "[" {
				return nil, nil
			}
			next()
			out := []string{}
			for {
				token := next()
				switch token {
				case "]":
					return out, nil
				case "":
					return nil, syntaxErr("expected ']'")
				default:
					out = append(out, token)
				}
			}
		}

		var bug string
		if strings.HasPrefix(peek(), "crbug.com") {
			bug = next()
		}
		testTags, err := tags()
		if err != nil {
			return nil, err
		}
		if t := peek(); t == "[" || t == "" {
			return nil, syntaxErr("expected test name")
		}
		test := next()
		result, err := tags()
		if err != nil {
			return nil, err
		}

		pending.Expectations = append(pending.Expectations, Expectation{
			Bug:     bug,
			Tags:    testTags,
			Test:    test,
			Results: result,
		})
	}

	if lastLineType != blank {
		flush()
	}

	return &Expectations{chunks}, nil
}
