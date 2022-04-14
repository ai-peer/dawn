package update

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/git"
	"dawn.googlesource.com/dawn/tools/src/gitiles"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"go.chromium.org/luci/auth"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
)

func init() {
	common.Register(&cmd{})
}

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
	rebuild bool // Rebuild the expectations file from scratch
	recycle bool // Re-use past roll changes
}

type cmd struct {
	flags rollerFlags
}

func (cmd) Name() string {
	return "roll"
}

func (cmd) Desc() string {
	return "roll CTS and re-generate expectations"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	gitPath, _ := exec.LookPath("git")
	tscPath, _ := exec.LookPath("tsc")
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	flag.StringVar(&c.flags.gitPath, "git", gitPath, "path to git")
	flag.StringVar(&c.flags.tscPath, "tsc", tscPath, "path to tsc")
	flag.BoolVar(&c.flags.rebuild, "rebuild", false, "rebuild the expectation file from scratch")
	flag.BoolVar(&c.flags.recycle, "recycle", false, "do not abandon existing rolls")

	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
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

	ctsDir := filepath.Join(tmpDir, "cts")

	gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
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
		ctsDir:   ctsDir,
	}

	return r.roll(ctx)
}

type roller struct {
	cfg      common.Config
	flags    rollerFlags
	auth     auth.Options
	bb       *buildbucket.Buildbucket
	rdb      *resultsdb.ResultsDB
	git      *git.Git
	gerrit   *gerrit.Gerrit
	chromium *gitiles.Gitiles
	dawn     *gitiles.Gitiles
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

	// Download the expectations file
	expectationsFile, err := r.dawn.DownloadFile(ctx, refMain, expectationsRelPath)
	if err != nil {
		return err
	}
	ex, err := expectations.Parse(expectationsFile)
	if err != nil {
		return fmt.Errorf("failed to load expectations: %v", err)
	}

	if r.flags.rebuild {
		rebuilt := ex.Clone()
		rebuilt.Chunks = rebuilt.Chunks[:0]
		for _, c := range ex.Chunks {
			switch {
			case c.IsBlankLine():
				rebuilt.MaybeAddBlankLine()
			case c.IsCommentOnly():
				rebuilt.Chunks = append(rebuilt.Chunks, c)
			}
		}
		ex = rebuilt
	}

	// Regenerate the typescript dependency list
	tsSources, err := r.genTSDepList(ctx)
	if err != nil {
		return fmt.Errorf("failed to generate ts_sources.txt: %v", err)
	}

	// Look for an existing gerrit change to update
	existingRolls, err := r.findExistingRolls()
	if err != nil {
		return err
	}

	// Abandon existing rolls, if -recycle was not passed
	if len(existingRolls) > 0 {
		if !r.flags.recycle {
			log.Printf("abandoning %v existing roll...", len(existingRolls))
			for _, change := range existingRolls {
				if err := r.gerrit.Abandon(change.ChangeID); err != nil {
					return err
				}
			}
			existingRolls = nil
		}
	}

	msg := fmt.Sprintf(`%v

Roll CTS to %v
Update ts_sources.txt
Regenerate expectations
`, ctsRollSubject, ctsHash)

	changeID := ""
	if len(existingRolls) == 0 {
		change, err := r.gerrit.CreateChange(r.cfg.Gerrit.Project, "main", msg, true)
		if err != nil {
			return err
		}
		changeID = change.ID
	} else {
		changeID = existingRolls[0].ID
		msg += fmt.Sprintf(`
Change-Id: %v`, existingRolls[0].ChangeID)
	}

	ps, err := r.gerrit.EditFiles(changeID, msg, map[string]string{
		depsRelPath:         updatedDEPS,
		expectationsRelPath: ex.String(),
		tsSourcesRelPath:    tsSources,
	})
	if err != nil {
		return fmt.Errorf("failed to update change '%v': %v", changeID, err)
	}

	const maxPasses = 5

	results := result.List{}
	for pass := 0; ; pass++ {
		log.Println("pass", pass)
		builds, err := common.GetBuilds(ctx, r.cfg, ps, r.bb, false)
		if err != nil {
			return err
		}

		failingBuilds := []string{}
		for id, build := range builds {
			if build.Status != buildbucket.StatusSuccess {
				failingBuilds = append(failingBuilds, id)
			}
		}
		if len(failingBuilds) == 0 {
			break
		}
		sort.Strings(failingBuilds)

		if pass >= maxPasses {
			err := fmt.Errorf("CTS failed after %v attempts.\nGiving up", pass)
			r.gerrit.Comment(ps, err.Error())
			return err
		}

		log.Println("gathering results...")
		psResults, err := common.GetResults(ctx, r.cfg, r.rdb, builds)
		if err != nil {
			return err
		}

		// Merge the new results into the accumulated results
		log.Println("merging results...")
		results = result.Merge(results, psResults)

		if false {
			f, err := os.Create("results.txt")
			if err == nil {
				defer f.Close()
				result.Write(f, results)
			}
		}

		log.Println("updating expectations...")
		newExpectations := ex.Clone()
		if diag, err := newExpectations.Update(results, &r.cfg.Expectations); err != nil {
			_ = diag // TODO
			return err
		}

		log.Println("updating expectations...")
		ps, err = r.gerrit.EditFiles(changeID, msg, map[string]string{
			expectationsRelPath: newExpectations.String(),
		})
		if err != nil {
			return fmt.Errorf("failed to update change '%v': %v", changeID, err)
		}

		log.Println("posting stats...")
		{
			counts := map[result.Status]int{}
			for _, r := range results {
				counts[r.Status] = counts[r.Status] + 1
			}
			stats := []string{}
			for s, n := range counts {
				if n > 0 {
					stats = append(stats, fmt.Sprintf("%v: %v", s, n))
				}
			}
			sort.Strings(stats)
			r.gerrit.Comment(ps, strings.Join(stats, "\n"))
		}
	}

	return nil
}

func (r *roller) findExistingRolls() ([]gerrit.ChangeInfo, error) {
	// Look for an existing gerrit change to update
	changes, _, err := r.gerrit.QueryChanges("owner:me",
		"is:open",
		fmt.Sprintf(`repo:"%v"`, r.cfg.Git.Dawn.Project),
		fmt.Sprintf(`message:"%v"`, ctsRollSubject))
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

	prefix := filepath.ToSlash(r.ctsDir) + "/"

	deps := []string{}
	for _, line := range strings.Split(string(out), "\n") {
		if strings.HasPrefix(line, prefix) {
			line = line[len(prefix):]
			if strings.HasPrefix(line, "src/") {
				deps = append(deps, line)
			}
		}
	}

	return strings.Join(deps, "\n") + "\n", nil
}
