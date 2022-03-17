package main

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
)

type cmdReduce struct {
	flags struct {
		output string
	}
}

func (cmdReduce) name() string { return "reduce" }
func (cmdReduce) desc() string { return `simplify results by collapsing common results` }
func (c *cmdReduce) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.output, "o", "", "output file. If unspecified writes to stdout")
	return []string{"<file>"}, nil
}
func (c *cmdReduce) run(ctx context.Context, cfg Config) error {
	path := flag.Arg(0)
	input, err := os.Open(path)
	if err != nil {
		return fmt.Errorf("failed to open '%v': %w", path, err)
	}
	defer input.Close()

	results, err := readResults(input)
	if err != nil {
		return err
	}

	// Open output file
	output := os.Stdout
	if c.flags.output != "" {
		output, err = os.Create(c.flags.output)
		if err != nil {
			return fmt.Errorf("failed to open output file '%v': %w", c.flags.output, err)
		}
		defer output.Close()
	}

	// Collapse all duplicate results
	results = results.ReplaceDuplicates(deduplicateResults)

	// Build a tree of the results
	tree, err := tree.New(results)
	if err != nil {
		return err
	}

	// Reduce the tree
	tree.Reduce()

	results = tree.AllResults()

	return writeResults(output, results)
}
