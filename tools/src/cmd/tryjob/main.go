// Copyright 2023 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// tryjob is a collection of sub-commands for managing tryjobs.
//
// To view available commands run: '<dawn>/tools/run tryjob --help'
package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/subcmd"

	// Register sub-commands
	_ "dawn.googlesource.com/dawn/tools/src/cmd/tryjob/cancel"
)

func main() {
	ctx := context.Background()

	cfg, err := common.LoadConfig(filepath.Join(fileutils.ThisDir(), "../cts/config.json"))
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
