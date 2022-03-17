package main

import (
	"context"
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
)

type cmdUpdate struct {
	flags struct {
		results      string
		expectations string
	}
}

func (cmdUpdate) name() string { return "update" }
func (cmdUpdate) desc() string { return "updates a CTS expectations file" }
func (c *cmdUpdate) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.results, "results", "results.txt", "path to CTS results file")
	flag.StringVar(&c.flags.expectations, "expectations", "expectations.txt", "path to CTS expectations file to update")
	return nil, nil
}
func (c *cmdUpdate) run(ctx context.Context, cfg Config) error {
	resultsFile, err := os.Open(c.flags.results)
	if err != nil {
		return err
	}
	defer resultsFile.Close()

	results, err := readResults(resultsFile)
	if err != nil {
		return err
	}

	results = deduplicate(results)

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
