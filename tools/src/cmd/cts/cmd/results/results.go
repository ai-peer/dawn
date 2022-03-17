package results

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strconv"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		retest bool // Force a re-test of the given patchsets?
		all    bool // Gather and merge results across all patchsets?
		cache  string
		output string
		ps     gerrit.Patchset
		auth   authcli.Flags
	}
}

func (cmd) Name() string {
	return "results"
}

func (cmd) Desc() string {
	return "obtains the CTS results for a patchset"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	flag.BoolVar(&c.flags.retest, "retest", false, "force another retest of the patchset")
	flag.BoolVar(&c.flags.all, "all", false, "gather results of all patchsets and merge results")
	flag.StringVar(&c.flags.cache, "cache", "", "cache directory")
	flag.StringVar(&c.flags.output, "o", "", "output file. If unspecified writes to stdout")
	c.flags.ps.RegisterFlags(cfg.Gerrit.Host, cfg.Gerrit.Project)
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Validate command line arguments
	opts, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	if c.flags.ps.Change == 0 {
		flag.Usage()
		return fmt.Errorf("missing --change flag")
	}

	// Open output file
	output := os.Stdout
	if c.flags.output != "" {
		output, err = os.Create(c.flags.output)
		if err != nil {
			return fmt.Errorf("failed to open output file '%v': %w", c.flags.output, err)
		}
		defer output.Close()
	}

	bb, err := buildbucket.New(ctx, opts)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, opts)
	if err != nil {
		return err
	}

	ps := c.flags.ps
	if c.flags.ps.Patchset == 0 || c.flags.all {
		gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
		if err != nil {
			return err
		}
		ps, err = gerrit.LatestPatchest(strconv.Itoa(c.flags.ps.Change))
		if err != nil {
			return fmt.Errorf("failed to find latest patchset of change %v: %w",
				c.flags.ps.Change, err)
		}
	}

	var cache *resultCache
	if c.flags.cache != "" {
		cache = &resultCache{c.flags.cache}
	}

	results := result.List{}
	if !c.flags.all {
		results, err = getBuildsResults(ctx, cfg, ps, bb, rdb, c.flags.retest, cache)
		if err != nil {
			return err
		}
	} else {
		psFrom, psTo := 1, ps.Patchset
		for ps.Patchset = psFrom; ps.Patchset <= psTo; ps.Patchset++ {
			psResults, err := getBuildsResults(ctx, cfg, ps, bb, rdb, c.flags.retest, cache)
			if err != nil {
				return err
			}
			results = result.Merge(results, psResults)
		}
	}

	return result.Write(output, results)
}

func getBuildsResults(
	ctx context.Context,
	cfg common.Config,
	ps gerrit.Patchset,
	bb *buildbucket.Buildbucket,
	rdb *resultsdb.ResultsDB,
	retest bool,
	cache *resultCache) (result.List, error) {

	log.Printf("gathering results from change %d, ps %d...", ps.Change, ps.Patchset)

	if cache != nil {
		results, err := cache.get(ps)
		if err != nil {
			log.Printf("resultCache.get() returned error: %v", err)
		} else if results != nil {
			return results, nil
		}
	}

	builds, err := common.GetBuilds(ctx, cfg, ps, bb, retest)
	if err != nil {
		return nil, err
	}
	results, err := common.GetResults(ctx, cfg, rdb, builds)
	if err != nil {
		return nil, err
	}
	if cache != nil {
		if err := cache.put(ps, results); err != nil {
			log.Printf("resultCache.put() returned error: %v", err)
		}
	}
	return results, nil
}

type resultCache struct {
	dir string
}

func (c resultCache) path(ps gerrit.Patchset) string {
	return filepath.Join(c.dir, strconv.Itoa(ps.Change), fmt.Sprintf("ps-%v.txt", ps.Patchset))
}

func (c resultCache) get(ps gerrit.Patchset) (result.List, error) {
	path := c.path(ps)
	if _, err := os.Stat(path); err == nil {
		return result.Load(path)
	}
	return nil, nil
}

func (c resultCache) put(ps gerrit.Patchset, results result.List) error {
	return result.Save(c.path(ps), results)
}
