package time

import (
	"context"
	"flag"
	"fmt"
	"math"
	"sort"
	"time"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/subcmd"
	"go.chromium.org/luci/auth/client/authcli"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		source    common.ResultSource
		auth      authcli.Flags
		tags      string
		topN      int
		histogram bool
	}
}

func (cmd) Name() string {
	return "time"
}

func (cmd) Desc() string {
	return "displays timing information for tests"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	c.flags.source.RegisterFlags(cfg)
	c.flags.auth.Register(flag.CommandLine, common.DefaultAuthOptions())
	flag.IntVar(&c.flags.topN, "top", 0, "print the top N slowest tests")
	flag.BoolVar(&c.flags.histogram, "histogram", false, "print a histogram of test timings")
	flag.StringVar(&c.flags.tags, "tags", "", "comma-separated list of tags to filter results")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Validate command line arguments
	auth, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	// Obtain the results
	results, err := c.flags.source.GetResults(ctx, cfg, auth)
	if err != nil {
		return err
	}

	if len(results) == 0 {
		return fmt.Errorf("no results found")
	}

	if c.flags.tags != "" {
		results = results.FilterByTags(result.StringToTags(c.flags.tags))
		if len(results) == 0 {
			return fmt.Errorf("no results after filtering by tags")
		}
	}

	sort.Slice(results, func(i, j int) bool {
		return results[i].Duration > results[j].Duration
	})

	didSomething := false

	if c.flags.topN > 0 {
		didSomething = true
		topN := results
		if c.flags.topN < len(results) {
			topN = topN[:c.flags.topN]
		}
		for i, r := range topN {
			fmt.Printf("%3.1d: %v\n", i, r)
		}
	}

	if c.flags.histogram {
		maxTime := results[0].Duration

		const (
			numBins = 25
			pow     = 2.0
		)

		binToDuration := func(i int) time.Duration {
			frac := math.Pow(float64(i)/float64(numBins), pow)
			return time.Duration(float64(maxTime) * frac)
		}
		durationToBin := func(d time.Duration) int {
			frac := math.Pow(float64(d)/float64(maxTime), 1.0/pow)
			idx := int(frac * numBins)
			if idx >= numBins-1 {
				return numBins - 1
			}
			return idx
		}

		didSomething = true
		bins := make([]int, numBins)
		for _, r := range results {
			idx := durationToBin(r.Duration)
			bins[idx] = bins[idx] + 1
		}
		for i, bin := range bins {
			fmt.Printf("[%.8v, %.8v]: %v\n", binToDuration(i), binToDuration(i+1), bin)
		}
	}

	if !didSomething {
		fmt.Fprintln(flag.CommandLine.Output(), "no action flags specified for", c.Name())
		fmt.Fprintln(flag.CommandLine.Output())
		flag.Usage()
		return subcmd.ErrInvalidCLA
	}

	return nil
}
