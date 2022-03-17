package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/subcmd"
	"dawn.googlesource.com/dawn/tools/src/utils"

	// Register sub-commands
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/export"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/format"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/merge"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/results"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/roll"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/time"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/update"
)

func main() {
	ctx := context.Background()

	cfg, err := common.LoadConfig(filepath.Join(utils.ThisDir(), "config.json"))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if err := subcmd.Run(ctx, *cfg, common.Commands()...); err != nil {
		if err != subcmd.ErrInvalidCLA {
			fmt.Fprintln(os.Stderr, err)
		}
		os.Exit(1)
	}
}
