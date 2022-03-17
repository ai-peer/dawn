package expectations

import (
	"errors"
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
)

func (e *Expectations) Update(l result.List) ([]string, error) {
	t, err := tree.New(l)
	if err != nil {
		return nil, err
	}

	msgs := []string{}
	newContent := []Chunk{}
	for _, chunk := range e.Content {
		if len(chunk.Expectations) == 0 {
			// Just a comment / blank line
			newContent = append(newContent, chunk)
			continue
		}

		filtered := Chunk{Comments: chunk.Comments}
		for _, e := range chunk.Expectations {
			name, err := query.Parse(e.Test)
			if err != nil {
				filtered.Expectations = append(filtered.Expectations, e)
				continue
			}

			var errNodeDoesNotExist tree.ErrNodeDoesNotExist
			r, err := t.Results(name)
			switch {
			case err == nil:
			case errors.As(err, &errNodeDoesNotExist):
				msgs = append(msgs, fmt.Sprintf("%v: Unknown test '%v'", e.Line, errNodeDoesNotExist.Query))
			default:
				return nil, err
			}

			if !r.All(result.Pass) {
				filtered.Expectations = append(filtered.Expectations, e)
			}
		}
		if len(filtered.Expectations) > 0 {
			newContent = append(newContent, filtered)
		}
	}

	e.Content = newContent
	return msgs, nil
}
