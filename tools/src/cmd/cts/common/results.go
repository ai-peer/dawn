package common

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"dawn.googlesource.com/dawn/tools/src/subcmd"
	"dawn.googlesource.com/dawn/tools/src/utils"
	"go.chromium.org/luci/auth"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
)

type ResultSource struct {
	CacheDir string
	File     string
	Patchset gerrit.Patchset
}

func (r *ResultSource) RegisterFlags(cfg Config) {
	flag.StringVar(&r.CacheDir, "cache", "~/.cache/webgpu-cts-results", "path to the results cache")
	flag.StringVar(&r.File, "results", "", "local results.txt file (mutually exclusive with --cl)")
	r.Patchset.RegisterFlags(cfg.Gerrit.Host, cfg.Gerrit.Project)
}

func (r *ResultSource) GetResults(ctx context.Context, cfg Config, auth auth.Options) (result.List, error) {
	if r.File == "" && r.Patchset.Change == 0 {
		fmt.Fprintln(flag.CommandLine.Output(), "one of --results and --cl must be specified")
		return nil, subcmd.ErrInvalidCLA
	}
	if r.File != "" && r.Patchset.Change != 0 {
		fmt.Fprintln(flag.CommandLine.Output(), "only one of --results and --cl can be specified")
		return nil, subcmd.ErrInvalidCLA
	}

	if r.File != "" {
		return result.Load(r.File)
	}

	var cachePath string
	if r.CacheDir != "" {
		dir := utils.ExpandHome(r.CacheDir)
		path := filepath.Join(dir, strconv.Itoa(r.Patchset.Change), fmt.Sprintf("ps-%v.txt", r.Patchset.Patchset))
		if _, err := os.Stat(path); err == nil {
			return result.Load(path)
		}
		cachePath = path
	}

	rdb, err := resultsdb.New(ctx, auth)
	if err != nil {
		return nil, err
	}

	bb, err := buildbucket.New(ctx, auth)
	if err != nil {
		return nil, err
	}

	if r.Patchset.Patchset == 0 {
		gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
		if err != nil {
			return nil, err
		}
		ps, err := gerrit.LatestPatchest(strconv.Itoa(r.Patchset.Change))
		if err != nil {
			err := fmt.Errorf("failed to find latest patchset of change %v: %w",
				r.Patchset.Change, err)
			return nil, err
		}
		r.Patchset.Patchset = ps.Patchset
	}

	log.Printf("fetching results from cl %v ps %v...", r.Patchset.Change, r.Patchset.Patchset)
	builds, err := GetBuilds(ctx, cfg, r.Patchset, bb, false)
	if err != nil {
		return nil, err
	}

	results, err := GetResults(ctx, cfg, rdb, builds)
	if err != nil {
		return nil, err
	}

	if err := result.Save(cachePath, results); err != nil {
		log.Println("failed to save results to cache: %w", err)
	}

	return results, nil
}

func GetResults(
	ctx context.Context,
	cfg Config,
	rdb *resultsdb.ResultsDB,
	builds BuildsByName) (result.List, error) {

	results := result.List{}
	err := rdb.QueryTestResults(ctx, builds.ids(), cfg.Test.Prefix+".*", func(rpb *rdbpb.TestResult) error {
		if !strings.HasPrefix(rpb.GetTestId(), cfg.Test.Prefix) {
			return nil
		}

		testName := rpb.GetTestId()[len(cfg.Test.Prefix):]
		status := toStatus(rpb.Status)
		tags := result.NewTags()

		for _, sp := range rpb.Tags {
			if sp.Key == "typ_tag" {
				tags.Add(sp.Value)
			}
		}

		if fr := rpb.GetFailureReason(); fr != nil {
			if strings.Contains(fr.GetPrimaryErrorMessage(), "asyncio.exceptions.TimeoutError") {
				status = result.Slow
			}
		}

		duration := rpb.GetDuration().AsDuration()
		if status == result.Pass && duration > cfg.Test.SlowThreshold {
			status = result.Slow
		}

		results = append(results, result.Result{
			Query:    query.Parse(testName),
			Status:   status,
			Tags:     tags,
			Duration: duration,
		})
		return nil
	})

	if err != nil {
		return nil, err
	}

	// Expand any aliased tags
	ExpandAliasedTags(cfg, results)

	// Remove any duplicates from the results.
	results = results.ReplaceDuplicates(result.Deduplicate)

	results.Sort()
	return results, err
}

// ExpandAliasedTags modifies each result so that tags which are found in
// cfg.TagAliases are expanded to include all the tag aliases.
// This is bodge for crbug.com/dawn/1387.
func ExpandAliasedTags(cfg Config, results result.List) {
	// Build the result sets
	sets := make([]result.Tags, len(cfg.TagAliases))
	for i, l := range cfg.TagAliases {
		sets[i] = result.NewTags(l...)
	}
	// Expand the result tags for the aliased tag sets
	for _, r := range results {
		for _, set := range sets {
			if r.Tags.ContainsAny(set) {
				r.Tags.AddAll(set)
			}
		}
	}
}

func toStatus(s rdbpb.TestStatus) result.Status {
	switch s {
	default:
		return result.Unknown
	case rdbpb.TestStatus_PASS:
		return result.Pass
	case rdbpb.TestStatus_FAIL:
		return result.Failure
	case rdbpb.TestStatus_CRASH:
		return result.Crash
	case rdbpb.TestStatus_ABORT:
		return result.Abort
	case rdbpb.TestStatus_SKIP:
		return result.Skip
	}
}
