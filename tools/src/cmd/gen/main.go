// Copyright 2021 The Tint Authors.
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

// gen parses the <tint>/src/tint/intrinsics.def file, then scans the
// project directory for '<file>.tmpl' files, to produce source code files.
package main

import (
	"flag"
	"fmt"
	"os"

	"dawn.googlesource.com/dawn/tools/src/cmd/gen/build"
)

func main() {
	if err := run(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func showUsage() {
	fmt.Println(`
gen generates the templated code for the Tint compiler

gen accepts a list of file paths to the templates to generate. If no templates
are explicitly specified, then gen scans the '<dawn>/src/tint' and
'<dawn>/test/tint' directories for '<file>.tmpl' files.

If the templates use the 'IntrinsicTable' function then gen will parse and
resolve the <tint>/src/tint/intrinsics.def file.

usage:
  gen [flags] [template files]

optional flags:`)
	flag.PrintDefaults()
	fmt.Println(``)
	os.Exit(1)
}

func run() error {
	outputDir := ""
	verbose := false
	dryRun := false
	flag.StringVar(&outputDir, "o", "", "custom output directory (optional)")
	flag.BoolVar(&verbose, "verbose", false, "print verbose output")
	flag.BoolVar(&dryRun, "dry", false, "don't emit anything, just check that files are up to date")
	flag.Parse()

	//  if err := intrinsics.Generate(outputDir, verbose, dryRun); err != nil {
	// }
	if err := build.Generate(verbose, dryRun); err != nil {
		return err
	}
	return nil
}
