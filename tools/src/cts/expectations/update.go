package expectations

import (
	"errors"
	"fmt"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"dawn.googlesource.com/dawn/tools/src/set"
)

type Diagnostic struct {
	Line    int
	Message string
}

func expandTags(tags []string, aliases map[string][]string) []string {
	for {
		expanded, changed := []string{}, false
		for _, t := range tags {
			if alias, ok := aliases[t]; ok {
				changed = true
				expanded = append(expanded, alias...)
			} else {
				expanded = append(expanded, t)
			}
		}
		tags = expanded
		if !changed {
			return tags
		}
	}
}

func expectationToResults(ex Expectation, cfg *Config, resultVariants result.Variants) (result.List, error) {
	query, err := query.Parse(strings.TrimPrefix(ex.Test, cfg.TestPrefix))
	if err != nil {
		return nil, err
	}

	type tagsUsed map[string]bool
	tags := map[TagType]tagsUsed{}

	tagNameToType, err := cfg.TagNameToType()
	if err != nil {
		return nil, err
	}

	for _, tag := range expandTags(ex.Tags, cfg.Tags.Aliases) {
		if ty, ok := tagNameToType[tag]; ok {
			if tags[ty] == nil {
				tags[ty] = tagsUsed{}
			}
			tags[ty][tag] = true
		} else {
			return nil, fmt.Errorf("unknown tag '%v'", tag)
		}
	}

	out := result.List{}
	for _, variant := range resultVariants {
		if len(tags[tagGPU]) > 0 && !tags[tagGPU][string(variant.GPU)] {
			continue
		}
		if len(tags[tagOS]) > 0 && !tags[tagOS][string(variant.OS)] {
			continue
		}
		for _, status := range ex.Status {
			out = append(out, result.Result{
				Query:   query,
				Variant: variant,
				Status:  result.Status(status),
			})
		}
	}

	return out, nil
}

func resultsToExpectations(results result.List, line int, bug string, cfg *Config) ([]Expectation, error) {
	results = results.Filter(func(r result.Result) bool {
		return r.Status != result.Pass
	})
	out := []Expectation{}
	for _, r := range results {
		out = append(out, Expectation{
			Line:   line,
			Bug:    bug,
			Tags:   []string{string(r.Variant.OS), string(r.Variant.GPU)},
			Test:   cfg.TestPrefix + r.Query.String(),
			Status: []string{string(r.Status)},
		})
	}
	return out, nil
}

func (e *Expectations) Update(results result.List, cfg *Config) ([]Diagnostic, error) {
	if len(results) == 0 {
		return nil, fmt.Errorf("no results provided")
	}

	// Check that all the results have tags declared for the OS and GPU
	{
		gpus, oses := set.From(cfg.Tags.GPU), set.From(cfg.Tags.OS)
		for _, r := range results {
			if !gpus.Contains(string(r.Variant.GPU)) {
				return nil, fmt.Errorf("GPU '%v' not found in expectation tags list", r.Variant.GPU)
			}
			if !oses.Contains(string(r.Variant.OS)) {
				return nil, fmt.Errorf("OS '%v' not found in expectation tags list", r.Variant.OS)
			}
		}
	}

	variants := results.Variants()

	tree, err := tree.New(results)
	if err != nil {
		return nil, err
	}

	return e.updateFromTree(tree, variants, cfg)
}

func (e *Expectations) updateFromTree(t *tree.Tree, variants result.Variants, cfg *Config) ([]Diagnostic, error) {
	const statusDeclared = `<<consumed>>`

	diags := []Diagnostic{}
	newContent := []Chunk{}
	for _, chunk := range e.Content {
		if len(chunk.Expectations) == 0 {
			// Just a comment / blank line
			newContent = append(newContent, chunk)
			continue
		}

		updated := Chunk{Comments: chunk.Comments}

		for _, ex := range chunk.Expectations {
			if !strings.HasPrefix(ex.Test, cfg.TestPrefix) {
				updated.Expectations = append(updated.Expectations, ex)
				continue
			}

			diag := func(msg string, args ...interface{}) {
				diags = append(diags, Diagnostic{ex.Line, fmt.Sprintf(msg, args...)})
			}

			exResults, err := expectationToResults(ex, cfg, variants)
			if err != nil {
				diag(err.Error())
				updated.Expectations = append(updated.Expectations, ex)
				continue
			}

			if len(exResults) == 0 {
				updated.Expectations = append(updated.Expectations, ex)
				continue
			}

			for _, exResult := range exResults {
				results, err := t.Results(exResult.Query, exResult.Variant)
				switch {
				case err == nil:
				case errors.As(err, &tree.ErrNoResultsForQuery{}):
					diag("'%v' was not tested for %v", exResult.Query, exResult.Variant)
					continue
				default:
					return nil, err
				}

				// If the query resolves to multiple sub-queries all with the
				// same status, then collapse these down to a single result.
				// No need to expand here.
				if len(results) > 0 && results.All(results[0].Status) {
					results = result.List{result.Result{
						Query:   exResult.Query,
						Variant: exResult.Variant,
						Status:  results[0].Status,
					}}
				}

				// Replace the status of all the expectation results with statusDeclared
				// This will be used to prevent the test being duplicated in the 'New expectations'
				// section at the bottom
				for _, result := range results {
					if err := t.Replace(exResult.Query, result.Variant, statusDeclared); err != nil {
						return nil, err
					}
				}
				newExpectations, err := resultsToExpectations(results, ex.Line, ex.Bug, cfg)
				if err != nil {
					diag("%v", err)
					continue
				}
				updated.Expectations = append(updated.Expectations, newExpectations...)
			}
		}

		if len(updated.Expectations) > 0 {
			newContent = append(newContent, updated)
		}
	}

	t.Reduce()

	// Filter out already declared expectations
	newResults := t.AllResults().Filter(func(r result.Result) bool {
		return r.Status != statusDeclared
	})

	newExpectations, err := resultsToExpectations(newResults, 0, "crbug.com/dawn/TODO", cfg)
	if err == nil {
		if len(newExpectations) > 0 {
			// Insert a blank line (if we don't end in one already)
			if len(newContent) > 0 && !newContent[len(newContent)-1].IsBlankLine() {
				newContent = append(newContent, Chunk{})
			}
			newContent = append(newContent, Chunk{
				Comments:     []string{"# New expectations. Please triage:"},
				Expectations: newExpectations,
			})
		}
	} else {
		diags = append(diags, Diagnostic{0, fmt.Sprint(err)})
	}

	e.Content = newContent
	return diags, nil
}
