package results

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"go.chromium.org/luci/auth/client/authcli"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		retest bool // Force a re-test of the given patchsets?
		all    bool // Gather and merge results across all patchsets?
		output string
		source common.ResultSource
		auth   authcli.Flags
	}
}

func (cmd) Name() string {
	return "results"
}

func (cmd) Desc() string {
	return "obtains the CTS results from a patchset"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	flag.BoolVar(&c.flags.retest, "retest", false, "force another retest of the patchset")
	flag.BoolVar(&c.flags.all, "all", false, "gather results of all patchsets and merge results")
	flag.StringVar(&c.flags.output, "o", "results.txt", "output file. '-' writes to stdout")
	c.flags.source.RegisterFlags(cfg)
	c.flags.auth.Register(flag.CommandLine, common.DefaultAuthOptions())
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

	// Open output file
	output := os.Stdout
	if c.flags.output != "-" {
		output, err = os.Create(c.flags.output)
		if err != nil {
			return fmt.Errorf("failed to open output file '%v': %w", c.flags.output, err)
		}
		defer output.Close()
	}

	return result.Write(output, results)
}
