package main

import (
	"context"
	"flag"

	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
)

type cmdFormat struct {
	flags struct {
	}
}

func (cmdFormat) name() string { return "format" }
func (cmdFormat) desc() string { return "formats a WebGPUExpectation file" }
func (c *cmdFormat) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	return []string{"<file>"}, nil
}
func (c *cmdFormat) run(ctx context.Context, cfg Config) error {
	path := flag.Args()[0]
	ex, err := expectations.Load(path)
	if err != nil {
		return err
	}
	return ex.Save(path)
}
