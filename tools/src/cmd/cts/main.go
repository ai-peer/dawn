package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/multicmd"
	"dawn.googlesource.com/dawn/tools/src/utils"

	// Register sub-commands
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/cmd/format"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/cmd/merge"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/cmd/results"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/cmd/roll"
	_ "dawn.googlesource.com/dawn/tools/src/cmd/cts/cmd/update"
)

func main() {
	ctx := context.Background()

	cfg, err := loadConfig(filepath.Join(utils.ThisDir(), "config.json"))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if err := multicmd.Run(ctx, *cfg, common.Commands()...); err != nil {
		if err != multicmd.ErrInvalidCLA {
			fmt.Fprintln(os.Stderr, err)
		}
		os.Exit(1)
	}
}

func loadConfig(path string) (*common.Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open '%v': %w", path, err)
	}
	defer f.Close()

	cfg := common.Config{}
	if err := json.NewDecoder(f).Decode(&cfg); err != nil {
		return nil, fmt.Errorf("failed to load config: %w", err)
	}
	return &cfg, nil
}
