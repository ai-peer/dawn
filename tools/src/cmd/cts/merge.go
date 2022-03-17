package main

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

type cmdMerge struct {
	flags struct {
		a, b   string
		output string
	}
}

func (cmdMerge) name() string { return "merges" }
func (cmdMerge) desc() string { return "merges two results files into one" }
func (c *cmdMerge) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.a, "a", "results-a.txt", "path to the first results file")
	flag.StringVar(&c.flags.b, "b", "results-b.txt", "path to the second results file")
	flag.StringVar(&c.flags.output, "o", "", "output file")
	return nil, nil
}
func (c *cmdMerge) run(ctx context.Context, cfg Config) error {
	// Load results A
	resultsA, err := loadResults(c.flags.a)
	if err != nil {
		return fmt.Errorf("while reading '%v': %w", c.flags.a, err)
	}

	// Load results B
	resultsB, err := loadResults(c.flags.b)
	if err != nil {
		return fmt.Errorf("while reading '%v': %w", c.flags.a, err)
	}

	// Combine and merge
	results := append(resultsA, resultsB...)
	results.ReplaceDuplicates(func(l result.List) result.Status {
		s := l.Statuses()
		switch {
		case s.Contains(result.Unknown):
			return result.Unknown
		case s.Contains(result.Crash):
			return result.Crash
		case s.Contains(result.Abort):
			return result.Abort
		case s.Contains(result.Failure):
			return result.Failure
		case s.Contains(result.Skip):
			return result.Skip
		case s.Contains(result.Pass):
			return result.Pass
		}
		return result.Unknown
	})

	// Open output file
	output := os.Stdout
	if c.flags.output != "" {
		output, err = os.Create(c.flags.output)
		if err != nil {
			return fmt.Errorf("failed to open output file '%v': %w", c.flags.output, err)
		}
		defer output.Close()
	}

	// Write out
	return writeResults(output, results)
}
