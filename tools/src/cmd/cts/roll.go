package main

import (
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/git"
	"dawn.googlesource.com/dawn/tools/src/gitiles"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"dawn.googlesource.com/dawn/tools/src/utils"
	"go.chromium.org/luci/auth"
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

	depsRelPath         = "DEPS"
	expectationsRelPath = "webgpu-cts/expectations.txt"
	tsSourcesRelPath    = "third_party/gn/webgpu-cts/ts_sources.txt"
	refMain             = "refs/heads/main"
	noExpectations      = `# Clear all expectations to obtain full list of results`
)

// The regular expression used to search for the CTS hash
var reCTSHash = regexp.MustCompile(reEscape(ctsHashPrefix) + `[0-9a-fA-F]+`)

type rollerFlags struct {
	gitPath string
	tscPath string
	auth    authcli.Flags
	keep    bool
}

type cmdRoll struct {
	flags rollerFlags
}

func (cmdRoll) name() string { return "roll" }
func (cmdRoll) desc() string { return "roll CTS and re-generate expectations" }
func (c *cmdRoll) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	gitPath, _ := exec.LookPath("git")
	tscPath, _ := exec.LookPath("tsc")
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	flag.StringVar(&c.flags.gitPath, "git", gitPath, "path to git")
	flag.StringVar(&c.flags.tscPath, "tsc", tscPath, "path to tsc")
	flag.BoolVar(&c.flags.keep, "keep", false, "do not abandon existing rolls")
	return nil, nil
}
func (c *cmdRoll) run(ctx context.Context, cfg Config) error {
	// Validate command line arguments
	auth, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	for _, tool := range []struct {
		name, path string
	}{
		{name: "git", path: c.flags.gitPath},
		{name: "tsc", path: c.flags.tscPath},
	} {
		if _, err := os.Stat(tool.path); err != nil {
			return fmt.Errorf("failed to find path to %v: %v", tool.name, err)
		}
	}

	git, err := git.New(c.flags.gitPath)
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	tmpDir, err := os.MkdirTemp("", "dawn-cts-roll")
	if err != nil {
		return err
	}
	defer os.RemoveAll(tmpDir)

	dawnDir := filepath.Join(tmpDir, "dawn")
	ctsDir := filepath.Join(tmpDir, "cts")

	gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Config{})
	if err != nil {
		return err
	}

	chromium, err := gitiles.New(ctx, cfg.Git.CTS.Host, cfg.Git.CTS.Project)
	if err != nil {
		return err
	}

	dawn, err := gitiles.New(ctx, cfg.Git.Dawn.Host, cfg.Git.Dawn.Project)
	if err != nil {
		return err
	}

	bb, err := buildbucket.New(ctx, auth)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, auth)
	if err != nil {
		return err
	}

	r := roller{
		cfg:      cfg,
		flags:    c.flags,
		auth:     auth,
		bb:       bb,
		rdb:      rdb,
		git:      git,
		gerrit:   gerrit,
		chromium: chromium,
		dawn:     dawn,
		dawnDir:  dawnDir,
		ctsDir:   ctsDir,
	}

	return r.roll(ctx)
}

type roller struct {
	cfg      Config
	flags    rollerFlags
	auth     auth.Options
	bb       *buildbucket.Buildbucket
	rdb      *resultsdb.ResultsDB
	git      *git.Git
	gerrit   *gerrit.Gerrit
	chromium *gitiles.Gitiles
	dawn     *gitiles.Gitiles
	dawnDir  string
	ctsDir   string
}

func (r *roller) roll(ctx context.Context) error {
	dawnHash, err := r.dawn.Hash(ctx, refMain)
	if err != nil {
		return err
	}

	updatedDEPS, ctsHash, err := r.updateDEPS(ctx, dawnHash)
	if err != nil {
		return err
	}
	if updatedDEPS == "" {
		// Already up to date
		return nil
	}

	log.Printf("starting CTS roll to %v...", ctsHash[:8])

	if _, err := r.checkout("cts", r.ctsDir, r.cfg.Git.CTS.HttpsURL(), ctsHash); err != nil {
		return err
	}

	dawnRepo, err := r.checkout("dawn", r.dawnDir, r.cfg.Git.Dawn.HttpsURL(), dawnHash)
	if err != nil {
		return err
	}

	// Load the expectations file
	expectationsPath := filepath.Join(r.dawnDir, expectationsRelPath)
	ex, err := expectations.Load(expectationsPath)
	if err != nil {
		return fmt.Errorf("failed to load expectations: %v", err)
	}

	// Update DEPS
	depsPath := filepath.Join(r.dawnDir, depsRelPath)
	if err := ioutil.WriteFile(depsPath, []byte(updatedDEPS), 0666); err != nil {
		return fmt.Errorf("failed to update DEPS: %v", err)
	}
	if err := dawnRepo.Add(depsPath, nil); err != nil {
		return fmt.Errorf("failed to stage DEPS: %v", err)
	}

	// Regenerate the typescript dependency list
	tsSources, err := r.genTSDepList(ctx)
	if err != nil {
		return fmt.Errorf("failed to generate ts_sources.txt: %v", err)
	}
	tsSourcesPath := filepath.Join(r.dawnDir, tsSourcesRelPath)
	err = os.WriteFile(tsSourcesPath, []byte(tsSources), 0666)
	if err != nil {
		return fmt.Errorf("failed to write to ts_sources.txt: %w", err)
	}
	if err := dawnRepo.Add(depsPath, nil); err != nil {
		return fmt.Errorf("failed to stage ts_sources.txt: %v", err)
	}

	// Look for an existing gerrit change to update
	existingRolls, err := r.findExistingRolls()
	if err != nil {
		return err
	}

	changeID := "I" + utils.Hash(dawnHash, ctsHash)

	if len(existingRolls) > 0 {
		if r.flags.keep {
			changeID = existingRolls[0].ChangeID
		} else {
			log.Printf("abandoning %v existing roll...", len(existingRolls))
			for _, change := range existingRolls {
				if err := r.gerrit.Abandon(change.ChangeID); err != nil {
					return err
				}
			}
		}
	}

	msg := fmt.Sprintf(`%v

Roll CTS to %v, update ts_sources.txt, regenerate expectations

Change-Id: %v
`, ctsRollSubject, ctsHash, changeID)
	commit, err := dawnRepo.Commit(msg, &git.CommitOptions{})
	if err != nil {
		return fmt.Errorf("failed to locally commit: %v", err)
	}

	if err := dawnRepo.Push(commit.String(), "refs/for/main", nil); err != nil {
		return fmt.Errorf("failed to push change to gerrit: %v", err)
	}

	existingRolls, err = r.findExistingRolls(fmt.Sprintf(`change:"%v"`, changeID))
	if err != nil {
		return err
	}

	change, err := r.findChange(existingRolls, changeID)
	if err != nil {
		return err
	}

	patchset := gerrit.Patchset{
		Host:     r.cfg.Gerrit.Host,
		Project:  r.cfg.Gerrit.Project,
		Change:   change.Number,
		Patchset: change.Revisions[change.CurrentRevision].Number,
	}

	builds, err := getBuilds(ctx, r.cfg, patchset, r.bb, true)
	if err != nil {
		return err
	}

	results, err := gatherBuildResults(ctx, r.cfg, r.rdb, builds)
	if err != nil {
		return err
	}

	if diag, err := ex.Update(results, &r.cfg.Expectations); err != nil {
		_ = diag // TODO
		return err
	}

	if err := ex.Save(expectationsPath); err != nil {
		return fmt.Errorf("failed to save updated expectations: %v", err)
	}

	if err := dawnRepo.Add(expectationsPath, nil); err != nil {
		return fmt.Errorf("failed to stage updated expectations: %v", err)
	}

	commit, err = dawnRepo.Commit("", &git.CommitOptions{Amend: true})
	if err != nil {
		return fmt.Errorf("failed to amend commit: %v", err)
	}

	if err := dawnRepo.Push(commit.String(), "refs/for/main", nil); err != nil {
		return fmt.Errorf("failed to push change to gerrit: %v", err)
	}

	return nil
}

func (r *roller) findExistingRolls(queries ...string) ([]gerrit.ChangeInfo, error) {
	queries = append(queries,
		"owner:me",
		"is:open",
		fmt.Sprintf(`repo:"%v"`, r.cfg.Git.Dawn.Project),
		fmt.Sprintf(`message:"%v"`, ctsRollSubject),
	)
	// Look for an existing gerrit change to update
	changes, _, err := r.gerrit.QueryChanges(queries...)
	if err != nil {
		return nil, fmt.Errorf("failed to find existing roll gerrit changes: %v", err)
	}
	return changes, nil
}

func (r *roller) findChange(changes []gerrit.ChangeInfo, changeID string) (gerrit.ChangeInfo, error) {
	for _, roll := range changes {
		if roll.ChangeID == changeID {
			return roll, nil
		}
	}
	return gerrit.ChangeInfo{}, fmt.Errorf("failed to find change %v", changeID)
}

func (r *roller) checkout(project, dir, host, sha string) (*git.Repository, error) {
	repo, err := r.git.Open(dir)
	if err != nil {
		log.Printf("cloning %v to '%v'...", project, dir)
		repo, err = r.git.Clone(dir, host, nil)
		if err != nil {
			return nil, fmt.Errorf("failed to clone %v: %v", project, err)
		}
	}

	log.Printf("fetching %v @ '%v'...", project, sha)
	if _, err := repo.Fetch(sha, nil); err != nil {
		return nil, fmt.Errorf("failed to fetch %v: %v", project, err)
	}

	log.Printf("checking out %v @ '%v'...", project, sha)
	if err := repo.Checkout(sha, nil); err != nil {
		return nil, fmt.Errorf("failed to clone %v: %v", project, err)
	}

	return repo, nil
}

func (r *roller) updateDEPS(ctx context.Context, dawnRef string) (newDEPS, ctsHash string, err error) {
	ctsHash, err = r.chromium.Hash(ctx, refMain)
	if err != nil {
		return "", "", err
	}

	deps, err := r.dawn.DownloadFile(ctx, dawnRef, depsRelPath)
	if err != nil {
		return "", "", err
	}

	split := reCTSHash.Split(deps, -1)
	if len(split) == 1 {
		return "", "", fmt.Errorf("failed to find CTS hash in DEPS file")
	}

	updatedDeps := strings.Join(split, ctsHashPrefix+ctsHash)

	if deps == updatedDeps {
		fmt.Println("CTS is already up to date")
		return "", "", nil
	}

	return updatedDeps, ctsHash, nil
}

func (r *roller) genTSDepList(ctx context.Context) (string, error) {
	cmd := exec.CommandContext(ctx, r.flags.tscPath, "--project",
		filepath.Join(r.ctsDir, "tsconfig.json"),
		"--listFiles",
		"--declaration", "false",
		"--sourceMap", "false")
	out, _ := cmd.Output()

	prefix := filepath.ToSlash(r.ctsDir)

	deps := []string{}
	for _, line := range strings.Split(string(out), "\n") {
		if strings.HasPrefix(line, prefix) {
			line = line[len(prefix):]
			if strings.HasPrefix(line, "src/") {
				deps = append(deps, line)
			}
		}
	}

	return strings.Join(deps, "\n"), nil
}
