package format

import (
	"context"
	"flag"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
	}
}

func (cmd) Name() string {
	return "format"
}

func (cmd) Desc() string {
	return "formats a WebGPUExpectation file"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	return []string{"<file>"}, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	path := flag.Args()[0]
	ex, err := expectations.Load(path)
	if err != nil {
		return err
	}
	return ex.Save(path)
}
