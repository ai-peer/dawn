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

package update

import (
	"context"
	"flag"
	"fmt"
	"log"
	"strconv"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"go.chromium.org/luci/auth/client/authcli"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		auth     authcli.Flags
		Patchset gerrit.Patchset
	}
}

func (cmd) Name() string {
	return "cancel"
}

func (cmd) Desc() string {
	return "cancel tryjob builds"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	c.flags.auth.Register(flag.CommandLine, common.DefaultAuthOptions())
	c.flags.Patchset.RegisterFlags(cfg.Gerrit.Host, cfg.Gerrit.Project)
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Validate command line arguments
	auth, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	// Initialize the buildbucket and resultdb clients
	bb, err := buildbucket.New(ctx, auth)
	if err != nil {
		return err
	}

	ps := &c.flags.Patchset
	if ps.Patchset == 0 {
		gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
		if err != nil {
			return err
		}
		*ps, err = gerrit.LatestPatchset(strconv.Itoa(ps.Change))
		if err != nil {
			return fmt.Errorf("failed to find latest patchset of change %v: %w",
				ps.Change, err)
		}
	}

	log.Printf("Searching builds on cl %v ps %v...", ps.Change, ps.Patchset)
	err = bb.SearchBuilds(ctx, *ps, func(build buildbucket.Build) error {
		switch build.Status {
		case buildbucket.StatusScheduled, buildbucket.StatusStarted:
			log.Printf("Canceling build %v on %v", build.ID, build.Builder)
			_, err = bb.CancelBuild(ctx, build.ID, "Developer no longer needs this tryjob")
			if err != nil {
				return err
			}
		}
		return nil
	})
	if err != nil {
		return err
	}
	log.Printf("Done")
	return nil
}
