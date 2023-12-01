// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"

	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/glob"
	"dawn.googlesource.com/dawn/tools/src/transform"
)

func main() {
	if err := run(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func showUsage() {
	fmt.Println(`
fuzz is a helper for running the tint fuzzer executables

usage:
  fuzz [flags...]`)
	flag.PrintDefaults()
	fmt.Println(``)
	os.Exit(1)
}

func run() error {
	t := tool{}

	check := false
	build := ""
	flag.BoolVar(&t.verbose, "verbose", false, "print additional output")
	flag.BoolVar(&check, "check", false, "check that all the end-to-end test do not fail")
	flag.StringVar(&t.corpus, "corpus", defaultCorpusDir(), "the corpus directory")
	flag.StringVar(&build, "build", defaultBuildDir(), "the build directory")
	flag.StringVar(&t.out, "out", "", "the directory to hold generated test files")
	flag.IntVar(&t.numProcesses, "j", runtime.NumCPU(), "number of concurrent fuzzers to run")
	flag.Parse()

	if t.numProcesses == 0 {
		t.numProcesses = 1
	}

	if !fileutils.IsDir(build) {
		return fmt.Errorf("build directory '%v' does not exist", build)
	}

	if t.out == "" {
		if tmp, err := os.MkdirTemp("", "tint_fuzz"); err == nil {
			defer os.RemoveAll(tmp)
			t.out = tmp
		} else {
			return err
		}
	}
	if !fileutils.IsDir(t.out) {
		return fmt.Errorf("output directory '%v' does not exist", t.out)
	}

	for _, fuzzer := range []struct {
		name string
		path *string
	}{
		{"tint_wgsl_fuzzer", &t.wgslFuzzer},
	} {
		*fuzzer.path = filepath.Join(build, fuzzer.name)
		if !fileutils.IsExe(*fuzzer.path) {
			return fmt.Errorf("fuzzer not found at '%v'", *fuzzer.path)
		}
	}

	if check {
		return t.check()
	}

	return t.run()
}

type tool struct {
	verbose      bool
	corpus       string // directory of test files
	out          string
	wgslFuzzer   string
	numProcesses int
}

func (t tool) check() error {
	files, err := glob.Glob(filepath.Join(t.corpus, "**"))
	if err != nil {
		return err
	}

	_, err = transform.GoSlice(files, func(path string) (any, error) {
		if out, err := exec.Command(t.wgslFuzzer, path).CombinedOutput(); err != nil {
			_, fuzzer := filepath.Split(t.wgslFuzzer)
			return nil, fmt.Errorf("%v run with %v failed with %v\n\n%v", fuzzer, path, err, string(out))
		}
		return nil, nil
	})
	if err != nil {
		return err
	}

	if t.verbose {
		log.Printf("checked %v files", len(files))
	}
	return err
}

func (t tool) run() error {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	errs := make(chan error, t.numProcesses)
	for i := 0; i < t.numProcesses; i++ {
		go func() {
			if out, err := exec.CommandContext(ctx, t.wgslFuzzer, t.out, t.corpus).CombinedOutput(); err != nil {
				_, fuzzer := filepath.Split(t.wgslFuzzer)
				errs <- fmt.Errorf("%v failed with %v\n\n%v", fuzzer, err, string(out))
			} else {
				panic(string(out))
			}
		}()
	}
	for err := range errs {
		return err
	}
	return nil
}

func defaultCorpusDir() string {
	return filepath.Join(fileutils.DawnRoot(), "test/tint")
}

func defaultBuildDir() string {
	return filepath.Join(fileutils.DawnRoot(), "out/active")
}
