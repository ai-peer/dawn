package expectations

import (
	"fmt"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
)

type Diagnostic struct {
	Line    int
	Message string
}

type tagType = string

const (
	tagOS                tagType = "OS"
	tagDevices           tagType = "Devices"
	tagPlatform          tagType = "Platform"
	tagBrowser           tagType = "Browser"
	tagGPU               tagType = "GPU"
	tagDecoder           tagType = "Decoder"
	tagANGLEBackend      tagType = "ANGLE Backend"
	tagSkiaRenderer      tagType = "Skia renderer"
	tagSwiftShader       tagType = "SwiftShader"
	tagDriver            tagType = "Driver"
	tagASan              tagType = "ASan"
	tagDisplayServer     tagType = "Display server"
	tagOOPCanvas         tagType = "OOP-Canvas"
	tagBackendValidation tagType = "Backend validation"
)

type tagNameToType map[string]tagType

func (c Config) tags() (tagNameToType, error) {
	tags := tagNameToType{}
	for _, t := range []struct {
		ty   tagType
		tags []string
	}{
		{tagOS, c.Tags.OS},
		{tagDevices, c.Tags.Devices},
		{tagPlatform, c.Tags.Platform},
		{tagBrowser, c.Tags.Browser},
		{tagGPU, c.Tags.GPU},
		{tagDecoder, c.Tags.Decoder},
		{tagANGLEBackend, c.Tags.ANGLEBackend},
		{tagSkiaRenderer, c.Tags.SkiaRenderer},
		{tagSwiftShader, c.Tags.SwiftShader},
		{tagDriver, c.Tags.Driver},
		{tagASan, c.Tags.ASan},
		{tagDisplayServer, c.Tags.DisplayServer},
		{tagOOPCanvas, c.Tags.OOPCanvas},
		{tagBackendValidation, c.Tags.BackendValidation},
	} {
		for _, tag := range t.tags {
			if _, collision := tags[tag]; collision {
				return nil, fmt.Errorf("Duplicate tag definition '%v'", tag)
			}
			tags[tag] = t.ty
		}
	}
	return tags, nil
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

func expectationToResults(ex Expectation, tagTypes tagNameToType, cfg Config, resultVariants result.Variants) (result.List, error) {
	query, err := query.Parse(strings.TrimPrefix(ex.Test, cfg.TestPrefix))
	if err != nil {
		return nil, err
	}

	type tagsUsed map[string]bool
	tags := map[tagType]tagsUsed{}

	for _, tag := range expandTags(ex.Tags, cfg.Tags.Aliases) {
		if ty, ok := tagTypes[tag]; ok {
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

func resultsToExpectations(results result.List, line int, bug string, cfg Config) ([]Expectation, error) {
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

func (e *Expectations) Update(results result.List, cfg Config) ([]Diagnostic, error) {
	if len(results) == 0 {
		return nil, fmt.Errorf("no results provided")
	}

	tags, err := cfg.tags()
	if err != nil {
		return nil, err
	}

	variants := results.Variants()

	t, err := tree.New(results)
	if err != nil {
		return nil, err
	}

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

			exResults, err := expectationToResults(ex, tags, cfg, variants)
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
				results := t.Results(exResult.Query, exResult.Variant)
				if len(results) == 0 {
					diag("no result found for query '%v' for %v", exResult.Query, exResult.Variant)
					continue
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

	e.Content = newContent
	return diags, nil
}
