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

package expectations

import (
	"fmt"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

const (
	tagHeaderStart = `START TAG HEADER`
	tagHeaderEnd   = `END TAG HEADER`
)

type ParserError struct {
	Line    int
	Column  int
	Message string
}

func (e ParserError) Error() string {
	return fmt.Sprintf("%v:%v: %v", e.Line, e.Column, e.Message)
}

// Parse parses an expectations file content
func Parse(body string) (Content, error) {
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

	content := Content{}
	var pending Chunk
	flush := func() {
		parseTags(&content.Tags, pending.Comments)
		content.Chunks = append(content.Chunks, pending)
		pending = Chunk{}
	}

	lastLineType := blank
	for i, l := range strings.Split(body, "\n") {
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

		tokens := removeEmpty(strings.Split(l, " "))
		column := 1

		syntaxErr := func(msg string) error {
			return ParserError{line, column, msg}
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
			column += len(tokens[0])
			tokens = tokens[1:]
			return t
		}
		tags := func() (result.Tags, error) {
			if peek() != "[" {
				return result.Tags{}, nil
			}
			next()
			out := result.NewTags()
			for {
				token := next()
				switch token {
				case "]":
					return out, nil
				case "":
					return result.Tags{}, syntaxErr("expected ']'")
				default:
					out.Add(token)
				}
			}
		}

		var bug string
		if strings.HasPrefix(peek(), "crbug.com") {
			bug = next()
		}
		testTags, err := tags()
		if err != nil {
			return Content{}, err
		}
		if t := peek(); t == "[" || t == "" {
			return Content{}, syntaxErr("expected test name")
		}
		query := next()
		status, err := tags()
		if err != nil {
			return Content{}, err
		}

		comment := ""
		if t := peek(); strings.HasPrefix(t, "#") {
			comment = strings.Join(tokens, " ")
		}

		pending.Expectations = append(pending.Expectations, Expectation{
			Line:    line,
			Bug:     bug,
			Tags:    testTags,
			Query:   query,
			Status:  status.List(),
			Comment: comment,
		})
	}

	if lastLineType != blank {
		flush()
	}

	return content, nil
}

func parseTags(tags *map[string]TagSetAndPriority, lines []string) {
	inTagsHeader, inList := false, false
	tagSet, priority := "", 0
	for _, line := range lines {
		line = strings.TrimSpace(strings.TrimLeft(strings.TrimSpace(line), "#"))
		if strings.Contains(line, `BEGIN TAG HEADER`) {
			if *tags == nil {
				*tags = map[string]TagSetAndPriority{}
			}
			inTagsHeader = true
			continue
		}
		if strings.Contains(line, `END TAG HEADER`) {
			return
		}
		if !inTagsHeader {
			continue
		}
		tokens := removeEmpty(strings.Split(line, " "))
	loop:
		for len(tokens) > 0 {
			switch {
			case inList && tokens[0] == "]":
				inList = false
			case inList:
				(*tags)[tokens[0]] = TagSetAndPriority{Set: tagSet, Priority: priority}
				priority++
			case !inList && len(tokens) > 2 && tokens[0] == "tags:" && tokens[1] == "[":
				inList = true
				tokens = tokens[2:]
				priority = 0
				continue
			case !inList:
				tagSet = strings.Join(tokens, " ")
				break loop
			}
			tokens = tokens[1:]
		}
	}
}

func removeEmpty(in []string) []string {
	out := make([]string, 0, len(in))
	for _, s := range in {
		if s != "" {
			out = append(out, s)
		}
	}
	return out
}
