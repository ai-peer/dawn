package main

import (
	"context"
	"flag"
	"fmt"
	"os"
)

type cmdMerge struct {
	flags struct {
		a, b   string
		output string
	}
}

func (cmdMerge) name() string { return "merge" }
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
		return fmt.Errorf("while reading '%v': %w", c.flags.b, err)
	}

	// Combine and merge
	results := mergeResults(resultsA, resultsB)

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
