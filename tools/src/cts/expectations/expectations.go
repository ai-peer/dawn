package expectations

import (
	"fmt"
	"io"
	"strings"
)

type Chunk struct {
	Comments     []string
	Expectations []Expectation
}

type Expectation struct {
	Bug     string
	Tags    []string
	Test    string
	Results []string
}

type Expectations struct {
	Content []Chunk
}

func (e *Expectations) Write(w io.Writer) error {
	for _, chunk := range e.Content {
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
				parts = append(parts, fmt.Sprintf("[ %v ]", strings.Join(expectation.Tags, " ")))
			}
			parts = append(parts, expectation.Test)
			parts = append(parts, fmt.Sprintf("[ %v ]", strings.Join(expectation.Results, " ")))
			if _, err := fmt.Fprintln(w, strings.Join(parts, " ")); err != nil {
				return err
			}
		}
	}
	return nil
}
