// Copyright 2022 The Dawn Authors
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

package validate

import (
	"context"
	"flag"
	"fmt"
	"os"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
)

func init() {
	common.Register(&cmd{})
}

type arrayFlags []string

func (i *arrayFlags) String() string {
	return strings.Join((*i), " ")
}

func (i *arrayFlags) Set(value string) error {
	*i = append(*i, value)
	return nil
}

type cmd struct {
	flags struct {
		expectations arrayFlags // expectations file path
		slow         string     // slow test expectations file path
	}
}

func (cmd) Name() string {
	return "validate"
}

func (cmd) Desc() string {
	return "validates a WebGPU expectations.txt file"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	slowExpectations := common.DefaultSlowExpectationsPath()
	flag.Var(&c.flags.expectations, "expectations", "path to CTS expectations file to validate")
	flag.StringVar(&c.flags.slow, "slow", slowExpectations, "path to CTS slow expectations file to validate")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	if len(c.flags.expectations) == 0 {
		c.flags.expectations = common.DefaultExpectationsPaths()
	}

	for _, expectationFilename := range c.flags.expectations {
		// Load expectations.txt
		content, err := expectations.Load(expectationFilename)
		if err != nil {
			return err
		}
		diags := content.Validate()

		// Print any diagnostics
		diags.Print(os.Stdout, expectationFilename)
		if numErrs := diags.NumErrors(); numErrs > 0 {
			return fmt.Errorf("%v errors found in %v", numErrs, expectationFilename)
		}
	}

	// Load slow_tests.txt
	content, err := expectations.Load(c.flags.slow)
	if err != nil {
		return err
	}

	diags := content.ValidateSlowTests()
	// Print any diagnostics
	diags.Print(os.Stdout, c.flags.expectations[0])
	if numErrs := diags.NumErrors(); numErrs > 0 {
		return fmt.Errorf("%v errors found", numErrs)
	}

	fmt.Println("no issues found")
	return nil
}
