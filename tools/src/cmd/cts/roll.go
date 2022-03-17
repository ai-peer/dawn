package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"regexp"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/gitiles"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
)

func reEscape(s string) string {
	return strings.ReplaceAll(strings.ReplaceAll(s, `/`, `\/`), `.`, `\.`)
}

const (
	ctsRollSubject = "Roll CTS and update expectations"

	// The string prefix for the CTS hash in the DEPs file, used for identifying
	// and updating the DEPS file.
	ctsHashPrefix = `{chromium_git}/external/github.com/gpuweb/cts@`
)

// The regular expression used to search for the CTS hash
var reCTSHash = regexp.MustCompile(reEscape(ctsHashPrefix) + `[0-9a-fA-F]+`)

type cmdRoll struct {
	flags struct {
		auth authcli.Flags
		keep bool
	}
}

func (cmdRoll) name() string { return "roll" }
func (cmdRoll) desc() string { return "roll CTS and re-generate expectations" }
func (c *cmdRoll) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	flag.BoolVar(&c.flags.keep, "keep", false, "do not abandon existing rolls")
	return nil, nil
}
func (c *cmdRoll) run(ctx context.Context, cfg Config) error {
	const (
		depsPath       = "DEPS"
		main           = "refs/heads/main"
		noExpectations = `# Clear all expectations to obtain full list of results`
	)

	// Validate command line arguments
	opts, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	g, err := gerrit.New(ctx, cfg.Gerrit.Host, cfg.Git.Dawn.Project, opts)
	if err != nil {
		return err
	}

	chromium, err := gitiles.New(ctx, cfg.Git.CTS.Host, cfg.Git.CTS.Project)
	if err != nil {
		return err
	}

	ctsHead, err := chromium.Hash(ctx, main)
	if err != nil {
		return err
	}

	dawn, err := gitiles.New(ctx, cfg.Git.Dawn.Host, cfg.Git.Dawn.Project)
	if err != nil {
		return err
	}

	deps, err := dawn.DownloadFile(ctx, main, depsPath)
	if err != nil {
		return err
	}

	split := reCTSHash.Split(deps, -1)
	if len(split) == 1 {
		return fmt.Errorf("failed to find CTS hash in DEPS file")
	}

	updatedDeps := strings.Join(split, ctsHashPrefix+ctsHead)

	if deps == updatedDeps {
		fmt.Println("CTS is already up to date")
		return nil
	}

	if !c.flags.keep {
		changes, err := g.ListChanges(ctx, fmt.Sprintf(`owner:me is:open message:"%v"`, ctsRollSubject))
		if err != nil {
			return err
		}
		log.Printf("abandoning %v existing roll...", len(changes))
		for _, change := range changes {
			if err := g.Abandon(ctx, gerrit.ChangeID(change.Number)); err != nil {
				return err
			}
		}
	}

	dawnHead, err := dawn.Hash(ctx, main)
	if err != nil {
		return err
	}

	cl, err := g.CreateChange(ctx, dawnHead, ctsRollSubject)
	if err != nil {
		return err
	}

	if err := g.EditFile(ctx, cl, deps, updatedDeps); err != nil {
		return fmt.Errorf("failed to update DEPS with latest CTS hash: %v", err)
	}

	if err := g.EditFile(ctx, cl, cfg.Expectations.Path, noExpectations); err != nil {
		return fmt.Errorf("failed to clear expectations: %v", err)
	}
	return nil
}
