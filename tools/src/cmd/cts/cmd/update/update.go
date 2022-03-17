package update

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		results      string
		expectations string
	}
}

func (cmd) Name() string {
	return "update"
}

func (cmd) Desc() string {
	return "updates a CTS expectations file"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	flag.StringVar(&c.flags.results, "results", "results.txt", "path to CTS results file")
	flag.StringVar(&c.flags.expectations, "expectations", "expectations.txt", "path to CTS expectations file to update")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	resultsFile, err := os.Open(c.flags.results)
	if err != nil {
		return err
	}
	defer resultsFile.Close()

	results, err := result.Read(resultsFile)
	if err != nil {
		return err
	}

	results = results.ReplaceDuplicates(result.Deduplicate)

	ex, err := expectations.Load(c.flags.expectations)
	if err != nil {
		return err
	}

	msgs, err := ex.Update(results, &cfg.Expectations)
	if err != nil {
		return err
	}
	for _, msg := range msgs {
		fmt.Printf("%v:%v %v\n", c.flags.expectations, msg.Line, msg.Message)
	}

	return ex.Save(c.flags.expectations)
}
