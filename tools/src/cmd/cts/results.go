package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"regexp"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
)

type cmdResults struct {
	flags struct {
		output string
		ps     gerrit.Patchset
		auth   authcli.Flags
	}
}

func (cmdResults) name() string { return "results" }
func (cmdResults) desc() string { return "obtains the CTS results for a patchset" }
func (c *cmdResults) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.output, "o", "", "output file. If unspecified writes to stdout")
	c.flags.ps.RegisterFlags(cfg.Gerrit.Host, cfg.Gerrit.Project)
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	return nil, nil
}
func (c *cmdResults) run(ctx context.Context, cfg Config) error {
	// Compile testName regex
	testName, err := regexp.Compile(cfg.TestNameRegex)
	if err != nil {
		return fmt.Errorf("failed to parse test name regex: %w", err)
	}

	// Validate command line arguments
	opts, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	if c.flags.ps.Change == 0 {
		flag.Usage()
		return fmt.Errorf("missing --change flag")
	}
	if c.flags.ps.Patchset == 0 {
		flag.Usage()
		return fmt.Errorf("missing --ps flag")
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

	// Obtain CTS results
	bb, err := buildbucket.New(ctx, opts)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, opts)
	if err != nil {
		return err
	}

	builds, err := getBuilds(ctx, cfg, c.flags.ps, bb)
	if err != nil {
		return err
	}

	results := result.List{}
	err = rdb.QueryTestResults(ctx, builds.ids(), cfg.TestFilter, func(rpb *rdbpb.TestResult) error {
		if matches := testName.FindStringSubmatch(rpb.TestId); len(matches) > 1 {
			defs := rpb.GetVariant().GetDef()
			q, err := query.Parse(matches[1])
			if err != nil {
				return err
			}
			if !q.IsWildcard() {
				q = q.AppendCases("*")
			}
			results = append(results, result.Result{
				Query:  q,
				Status: toStatus(rpb.Status),
				Variant: result.Variant{
					GPU: result.GPU(defs["gpu"]),
					OS:  result.OS(defs["os"]),
				},
			})
		}
		return nil
	})
	if err != nil {
		return err
	}

	results.Sort()

	return writeResults(output, results)
}
