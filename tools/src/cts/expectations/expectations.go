package expectations

import (
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type Chunk struct {
	Comments     []string
	Expectations []Expectation
}

func (c Chunk) IsBlankLine() bool {
	return len(c.Comments) == 0 && len(c.Expectations) == 0
}

type Expectation struct {
	Line   int
	Bug    string
	Tags   result.Tags
	Test   string
	Status []string
}

type Content struct {
	Chunks []Chunk
	Tags   result.Tags
}

func Load(path string) (Content, error) {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return Content{}, err
	}
	ex, err := Parse(path, string(content))
	if err != nil {
		return Content{}, err
	}
	return ex, nil
}

func (c Content) Save(path string) error {
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	return c.Write(f)
}

// Returns true if the expectation
func (c Content) Empty() bool {
	return len(c.Chunks) == 0
}

func (c Content) EndsInBlankLine() bool {
	return !c.Empty() && c.Chunks[len(c.Chunks)-1].IsBlankLine()
}

func (c *Content) MaybeAddBlankLine() {
	if !c.Empty() && !c.EndsInBlankLine() {
		c.Chunks = append(c.Chunks, Chunk{})
	}
}

func (c Content) Write(w io.Writer) error {
	for _, chunk := range c.Chunks {
		if len(chunk.Comments) == 0 && len(chunk.Expectations) == 0 {
			if _, err := fmt.Fprintln(w); err != nil {
				return err
			}
			continue
		}
		for _, comment := range chunk.Comments {
			if _, err := fmt.Fprintln(w, comment); err != nil {
				return err
			}
		}
		for _, expectation := range chunk.Expectations {
			parts := []string{}
			if expectation.Bug != "" {
				parts = append(parts, expectation.Bug)
			}
			if len(expectation.Tags.Set) > 0 {
				parts = append(parts, fmt.Sprintf("[ %v ]", strings.Join(expectation.Tags.List(), " ")))
			}
			parts = append(parts, expectation.Test)
			parts = append(parts, fmt.Sprintf("[ %v ]", strings.Join(expectation.Status, " ")))
			if _, err := fmt.Fprintln(w, strings.Join(parts, " ")); err != nil {
				return err
			}
		}
	}
	return nil
}

func (c Content) String() string {
	sb := strings.Builder{}
	c.Write(&sb)
	return sb.String()
}
