package common

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

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
	flag.StringVar(&r.CacheDir, "cache", DefaultCacheDir, "path to the results cache")
	flag.StringVar(&r.File, "results", "", "local results.txt file (mutually exclusive with --cl)")
	r.Patchset.RegisterFlags(cfg.Gerrit.Host, cfg.Gerrit.Project)
}

func (r *ResultSource) GetResults(ctx context.Context, cfg Config, auth auth.Options) (result.List, error) {
	ps := r.Patchset
	if r.File != "" && ps.Change != 0 {
		fmt.Fprintln(flag.CommandLine.Output(), "only one of --results and --cl can be specified")
		return nil, subcmd.ErrInvalidCLA
	}

	if r.File != "" {
		return result.Load(r.File)
	}

	bb, err := buildbucket.New(ctx, auth)
	if err != nil {
		return nil, err
	}

	rdb, err := resultsdb.New(ctx, auth)
	if err != nil {
		return nil, err
	}

	if ps.Change == 0 {
		fmt.Println("no change specified, scanning gerrit for last CTS roll...")
		gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
		if err != nil {
			return nil, err
		}
		latest, err := LatestCTSRoll(gerrit)
		if err != nil {
			return nil, err
		}
		results, ps, err := MostRecentResultsForChange(ctx, cfg, r.CacheDir, gerrit, bb, rdb, latest.Number)
		if err != nil {
			return nil, err
		}
		fmt.Printf("using results from cl %v ps %v...\n", ps.Change, ps.Patchset)
		return results, nil
	}

	if ps.Patchset == 0 {
		gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
		if err != nil {
			return nil, err
		}
		ps, err := gerrit.LatestPatchest(strconv.Itoa(ps.Change))
		if err != nil {
			err := fmt.Errorf("failed to find latest patchset of change %v: %w",
				ps.Change, err)
			return nil, err
		}
	}

	log.Printf("fetching results from cl %v ps %v...", ps.Change, ps.Patchset)
	builds, err := GetOrStartBuilds(ctx, cfg, ps, bb, false)
	if err != nil {
		return nil, err
	}

	results, err := CacheResults(ctx, cfg, ps, r.CacheDir, rdb, builds)
	if err != nil {
		return nil, err
	}

	return results, nil
}

func CacheResults(
	ctx context.Context,
	cfg Config,
	ps gerrit.Patchset,
	cacheDir string,
	rdb *resultsdb.ResultsDB,
	builds BuildsByName) (result.List, error) {

	var cachePath string
	if cacheDir != "" {
		dir := utils.ExpandHome(cacheDir)
		path := filepath.Join(dir, strconv.Itoa(ps.Change), fmt.Sprintf("ps-%v.txt", ps.Patchset))
		if _, err := os.Stat(path); err == nil {
			return result.Load(path)
		}
		cachePath = path
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

	fmt.Printf("fetching results from resultdb...")

	lastDot := time.Now()

	results := result.List{}
	err := rdb.QueryTestResults(ctx, builds.ids(), cfg.Test.Prefix+".*", func(rpb *rdbpb.TestResult) error {
		if time.Since(lastDot) > 5*time.Second {
			lastDot = time.Now()
			fmt.Printf(".")
		}

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

	fmt.Println(" done")

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

func LatestCTSRoll(g *gerrit.Gerrit) (gerrit.ChangeInfo, error) {
	changes, _, err := g.QueryChanges(
		`status:merged`,
		`-age:1month`,
		fmt.Sprintf(`message:"%v"`, RollSubjectPrefix))
	if err != nil {
		return gerrit.ChangeInfo{}, err
	}
	if len(changes) == 0 {
		return gerrit.ChangeInfo{}, fmt.Errorf("no change found")
	}
	sort.Slice(changes, func(i, j int) bool {
		return changes[i].Submitted.Time.After(changes[j].Submitted.Time)
	})
	return changes[0], nil
}

func LatestPatchset(g *gerrit.Gerrit, change int) (gerrit.Patchset, error) {
	ps, err := g.LatestPatchest(strconv.Itoa(change))
	if err != nil {
		err := fmt.Errorf("failed to find latest patchset of change %v: %w",
			ps.Change, err)
		return gerrit.Patchset{}, err
	}
	return ps, nil
}

func MostRecentResultsForChange(
	ctx context.Context,
	cfg Config,
	cacheDir string,
	g *gerrit.Gerrit,
	bb *buildbucket.Buildbucket,
	rdb *resultsdb.ResultsDB,
	change int) (result.List, gerrit.Patchset, error) {

	ps, err := LatestPatchset(g, change)
	if err != nil {
		return nil, gerrit.Patchset{}, nil
	}

	for ps.Patchset > 1 {
		builds, err := GetBuilds(ctx, cfg, ps, bb)
		if err != nil {
			return nil, gerrit.Patchset{}, err
		}
		if len(builds) > 0 {
			if err := WaitForBuildsToComplete(ctx, cfg, ps, bb, builds); err != nil {
				return nil, gerrit.Patchset{}, err
			}

			results, err := CacheResults(ctx, cfg, ps, cacheDir, rdb, builds)
			if err != nil {
				return nil, gerrit.Patchset{}, err
			}

			if len(results) > 0 {
				return results, ps, nil
			}
		}
		ps.Patchset--
	}

	return nil, gerrit.Patchset{}, fmt.Errorf("no builds found for change %v", change)
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
