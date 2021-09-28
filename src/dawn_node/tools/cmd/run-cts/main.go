// Copyright 2021 The Dawn Authors
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

// idlgen is a tool used to generate code from WebIDL files and a golang
// template file
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)

const (
	testTimeout = time.Minute
)

func main() {
	if err := run(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func showUsage() {
	fmt.Println(`
run-cts is a tool used to run the WebGPU CTS using the Dawn module for NodeJS

Usage:
  run-cts --dawn-node=<path to dawn.node> --cts=<path to WebGPU CTS> [test-query]`)
	os.Exit(1)
}

func run() error {
	var dawnNode, cts, node, npx string
	var verbose, build bool
	flag.StringVar(&dawnNode, "dawn-node", "", "path to dawn.node module")
	flag.StringVar(&cts, "cts", "", "root directory of WebGPU CTS")
	flag.StringVar(&node, "node", "", "path to node executable")
	flag.StringVar(&npx, "npx", "", "path to npx executable")
	flag.BoolVar(&verbose, "verbose", false, "print extra information while testing")
	flag.BoolVar(&build, "build", true, "attempt to build the CTS before running")
	flag.Parse()

	// Check mandatory arguments
	if dawnNode == "" || cts == "" {
		showUsage()
	}
	if !IsFile(dawnNode) {
		return fmt.Errorf("'%v' is not a file", dawnNode)
	}
	if !IsDir(cts) {
		return fmt.Errorf("'%v' is not a directory", cts)
	}

	// Make paths absolute
	for _, path := range []*string{&dawnNode, &cts} {
		abs, err := filepath.Abs(*path)
		if err != nil {
			return fmt.Errorf("unable to get absolute path for '%v'", *path)
		}
		*path = abs
	}

	// The test query is the optional unnamed argument
	queries := []string{"webgpu:*"}
	if args := flag.Args(); len(args) > 0 {
		queries = args
	}

	// Find node
	if node == "" {
		var err error
		node, err = exec.LookPath("node")
		if err != nil {
			return fmt.Errorf("add node to PATH or specify with --node")
		}
	}
	// Find npx
	if npx == "" {
		var err error
		npx, err = exec.LookPath("npx")
		if err != nil {
			npx = ""
		}
	}

	r := runner{
		verbose:  verbose,
		node:     node,
		npx:      npx,
		dawnNode: dawnNode,
		cts:      cts,
		evalScript: `require('./src/common/tools/setup-ts-in-node.js');
		  require('./src/common/runtime/cmdline.ts');`,
	}
	if build {
		if npx != "" {
			if err := r.buildCTS(); err != nil {
				return fmt.Errorf("failed to build CTS: %w", err)
			} else {
				r.evalScript = `require('./out-node/common/runtime/cmdline.js');`
			}
		} else {
			fmt.Println("npx not found on PATH. Using runtime TypeScript transpilation (slow)")
		}
	}

	if err := r.gatherTestCases(queries); err != nil {
		return fmt.Errorf("failed to gather test cases: %w", err)
	}

	fmt.Printf("Testing %d test cases...\n", len(r.testcases))

	return r.run()
}

type runner struct {
	verbose                  bool
	node, npx, dawnNode, cts string
	evalScript               string
	testcases                []string
}

func (r *runner) buildCTS() error {
	cmd := exec.Command(r.npx, "grunt", "run:build-out-node")
	cmd.Dir = r.cts
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("%w: %v", err, string(out))
	}
	return nil
}

func (r *runner) gatherTestCases(queries []string) error {
	args := append([]string{
		"-e", r.evalScript,
		"--", // Start of arguments
		// src/common/runtime/helper/sys.ts expects 'node file.js <args>'
		// and slices away the first two arguments. When running with '-e', args
		// start at 1, so just inject a dummy argument.
		"dummy-arg",
		"--list",
	}, queries...)

	cmd := exec.Command(r.node, args...)
	cmd.Dir = r.cts
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("%w\n%v", err, string(out))
	}

	tests := filterTestcases(strings.Split(string(out), "\n"))
	r.testcases = tests
	return nil
}

func (r *runner) run() error {
	caseIndices := make(chan int, len(r.testcases))
	for i := range r.testcases {
		caseIndices <- i
	}
	close(caseIndices)

	results := make(chan result, len(r.testcases))

	start := time.Now()
	wg := sync.WaitGroup{}
	for i := 0; i < runtime.NumCPU(); i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for idx := range caseIndices {
				results <- r.runTestcase(idx)
			}
		}()
	}
	var timeTaken time.Duration
	go func() {
		wg.Wait()
		timeTaken = time.Since(start)
		close(results)
	}()

	numTests, numByStatus := len(r.testcases), map[status]int{}
	lastStatusUpdate, animFrame := time.Now(), 0
	updateProgress := func() {
		printANSIProgressBar(animFrame, numTests, numByStatus)
		animFrame++
		lastStatusUpdate = time.Now()
	}
	for res := range results {
		name := res.testcase
		numByStatus[res.status] = numByStatus[res.status] + 1
		if r.verbose || res.status != pass {
			fmt.Printf("%v - %v: %v\n", name, res.status, res.message)
			updateProgress()
		}
		if time.Since(lastStatusUpdate) > time.Millisecond*10 {
			updateProgress()
		}
	}
	printANSIProgressBar(animFrame, numTests, numByStatus)

	fmt.Printf(`
Completed in %v

pass:    %v (%v)
fail:    %v (%v)
skip:    %v (%v)
timeout: %v (%v)
`,
		timeTaken,
		numByStatus[pass], percentage(numByStatus[pass], numTests),
		numByStatus[fail], percentage(numByStatus[fail], numTests),
		numByStatus[skip], percentage(numByStatus[skip], numTests),
		numByStatus[timeout], percentage(numByStatus[timeout], numTests),
	)
	return nil
}

type status string

const (
	pass    status = "pass"
	fail    status = "fail"
	skip    status = "skip"
	timeout status = "timeout"
)

type result struct {
	testcase string
	status   status
	message  string
	error    error
}

func (r *runner) runTestcase(idx int) result {
	ctx, cancel := context.WithTimeout(context.Background(), testTimeout)
	defer cancel()

	testcase := r.testcases[idx]

	eval := r.evalScript
	args := append([]string{
		"-e", eval, // Evaluate 'eval'.
		"--", // Start of arguments
		// src/common/runtime/helper/sys.ts expects 'node file.js <args>'
		// and slices away the first two arguments. When running with '-e', args
		// start at 1, so just inject a dummy argument.
		"dummy-arg",
		// Actual arguments begin here
		"--gpu-provider", r.dawnNode,
		"--verbose",
	}, r.testcases[idx])

	cmd := exec.CommandContext(ctx, r.node, args...)
	cmd.Dir = r.cts
	out, err := cmd.CombinedOutput()
	msg := string(out)
	switch {
	case errors.Is(err, context.DeadlineExceeded):
		return result{testcase: testcase, status: timeout, message: msg}
	case strings.Contains(msg, "[fail]"):
		return result{testcase: testcase, status: fail, message: msg}
	case strings.Contains(msg, "[skip]"):
		return result{testcase: testcase, status: skip, message: msg}
	case strings.Contains(msg, "[pass]"), err == nil:
		return result{testcase: testcase, status: pass, message: msg}
	}
	return result{testcase: testcase, status: fail, message: fmt.Sprint(msg, err), error: err}
}

func filterTestcases(in []string) []string {
	out := make([]string, 0, len(in))
	for _, c := range in {
		if c != "" {
			out = append(out, c)
		}
	}
	return out
}

// percentage returns the percentage of n out of total as a string
func percentage(n, total int) string {
	if total == 0 {
		return "-"
	}
	f := float64(n) / float64(total)
	return fmt.Sprintf("%.1f%c", f*100.0, '%')
}

// IsDir returns true if the path resolves to a directory
func IsDir(path string) bool {
	s, err := os.Stat(path)
	if err != nil {
		return false
	}
	return s.IsDir()
}

// IsFile returns true if the path resolves to a file
func IsFile(path string) bool {
	s, err := os.Stat(path)
	if err != nil {
		return false
	}
	return !s.IsDir()
}

func printANSIProgressBar(animFrame int, numTests int, numByStatus map[status]int) {
	const (
		barWidth = 50

		escape       = "\u001B["
		positionLeft = escape + "G"
		red          = escape + "31m"
		green        = escape + "32m"
		yellow       = escape + "33m"
		blue         = escape + "34m"
		magenta      = escape + "35m"
		cyan         = escape + "36m"
		white        = escape + "37m"
		reset        = escape + "0m"
	)
	defer fmt.Print(positionLeft)

	animSymbols := []rune{'⣾', '⣽', '⣻', '⢿', '⡿', '⣟', '⣯', '⣷'}
	blockSymbols := []rune{'▏', '▎', '▍', '▌', '▋', '▊', '▉'}

	numBlocksPrinted := 0

	fmt.Print(string(animSymbols[animFrame%len(animSymbols)]), " [")
	animFrame++

	numFinished := 0

	for _, ty := range []struct {
		status status
		color  string
	}{{pass, green}, {skip, blue}, {timeout, yellow}, {fail, red}} {
		num := numByStatus[ty.status]
		numFinished += num
		statusFrac := float64(num) / float64(numTests)
		fNumBlocks := barWidth * statusFrac
		fmt.Print(ty.color)
		numBlocks := int(math.Ceil(fNumBlocks))
		if numBlocks > 1 {
			fmt.Print(strings.Repeat(string("▉"), numBlocks))
		}
		if numBlocks > 0 {
			frac := fNumBlocks - math.Floor(fNumBlocks)
			symbol := blockSymbols[int(math.Round(frac*float64(len(blockSymbols)-1)))]
			fmt.Print(string(symbol))
		}
		numBlocksPrinted += numBlocks
	}

	if barWidth > numBlocksPrinted {
		fmt.Print(strings.Repeat(string(" "), barWidth-numBlocksPrinted))
	}
	fmt.Print(reset)
	fmt.Print("] ", percentage(numFinished, numTests))
}
