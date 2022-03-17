package expectations

import (
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type Content struct {
	// The chunks that make up the expectations file
	Chunks []Chunk
	// Map of tag name to tag set and priority
	Tags map[string]TagSetAndPriority
}

type Chunk struct {
	Comments     []string
	Expectations []Expectation
}

type TagSetAndPriority struct {
	Set      string
	Priority int
}

type Expectation struct {
	Line   int
	Bug    string
	Tags   result.Tags
	Test   string
	Status []string
}

func Load(path string) (Content, error) {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return Content{}, err
	}
	ex, err := Parse(string(content))
	if err != nil {
		return Content{}, err
	}
	return ex, nil
}

func (c Content) Clone() Content {
	chunks := make([]Chunk, len(c.Chunks))
	for i, c := range c.Chunks {
		chunks[i] = c.Clone()
	}
	tags := make(map[string]TagSetAndPriority, len(c.Tags))
	for n, t := range c.Tags {
		tags[n] = t
	}
	return Content{chunks, tags}
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
			if len(expectation.Tags) > 0 {
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

func (c Chunk) IsCommentOnly() bool {
	return len(c.Comments) > 0 && len(c.Expectations) == 0
}

func (c Chunk) IsBlankLine() bool {
	return len(c.Comments) == 0 && len(c.Expectations) == 0
}

func (c Chunk) Clone() Chunk {
	comments := make([]string, len(c.Comments))
	for i, c := range c.Comments {
		comments[i] = c
	}
	expectations := make([]Expectation, len(c.Expectations))
	for i, e := range c.Expectations {
		expectations[i] = e
	}
	return Chunk{comments, expectations}
}
