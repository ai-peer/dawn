package merge

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		a, b   string
		output string
	}
}

func (cmd) Name() string { return "merge" }

func (cmd) Desc() string { return "merges two results files into one" }

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	flag.StringVar(&c.flags.a, "a", "results-a.txt", "path to the first results file")
	flag.StringVar(&c.flags.b, "b", "results-b.txt", "path to the second results file")
	flag.StringVar(&c.flags.output, "o", "", "output file")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Load results A
	resultsA, err := result.Load(c.flags.a)
	if err != nil {
		return fmt.Errorf("while reading '%v': %w", c.flags.a, err)
	}

	// Load results B
	resultsB, err := result.Load(c.flags.b)
	if err != nil {
		return fmt.Errorf("while reading '%v': %w", c.flags.b, err)
	}

	// Combine and merge
	results := result.Merge(resultsA, resultsB)

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
	return result.Write(output, results)
}
