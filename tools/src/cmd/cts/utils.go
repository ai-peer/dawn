package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"time"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
)

func loadConfig(path string) (*Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open '%v': %w", path, err)
	}
	defer f.Close()

	cfg := Config{}
	if err := json.NewDecoder(f).Decode(&cfg); err != nil {
		return nil, fmt.Errorf("failed to load config: %w", err)
	}
	return &cfg, nil
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

func thisDir() string {
	_, file, _, ok := runtime.Caller(1)
	if !ok {
		return ""
	}
	return filepath.Dir(file)
}

// BuildsByName is a map of builder name to build result
type BuildsByName map[string]buildbucket.Build

func (b BuildsByName) ids() []buildbucket.BuildID {
	ids := make([]buildbucket.BuildID, 0, len(b))
	for _, build := range b {
		ids = append(ids, build.ID)
	}
	return ids
}

func getBuilds(
	ctx context.Context,
	cfg Config,
	ps gerrit.Patchset,
	bb *buildbucket.Buildbucket,
	retest bool) (BuildsByName, error) {

	builds := BuildsByName{}

	if !retest {
		// Find any existing builds for the patchset
		err := bb.SearchBuilds(ctx, ps, func(build buildbucket.Build) error {
			for name, builder := range cfg.Builders {
				if build.Builder == builder {
					builds[name] = build
					break
				}
			}
			return nil
		})
		if err != nil {
			return nil, err
		}
	}

	// Kick any missing builds
	for name, builder := range cfg.Builders {
		if _, existing := builds[name]; !existing {
			build, err := bb.StartBuild(ctx, ps, builder, retest)
			if err != nil {
				return nil, err
			}
			log.Printf("started build: %+v", build)
			builds[name] = build
		}
	}

	buildsStillRunning := func() []string {
		out := []string{}
		for name, build := range builds {
			if build.Status.Running() {
				out = append(out, name)
			}
		}
		sort.Strings(out)
		return out
	}

	for {
		// Refresh build status
		for name, build := range builds {
			build, err := bb.QueryBuild(ctx, build.ID)
			if err != nil {
				return nil, fmt.Errorf("failed to query build for '%v': %w", name, err)
			}
			builds[name] = build
		}
		running := buildsStillRunning()
		if len(running) == 0 {
			break
		}
		log.Println("waiting for builds to complete: ", running)
		time.Sleep(time.Minute * 2)
	}

	for name, build := range builds {
		if build.Status == buildbucket.StatusInfraFailure ||
			build.Status == buildbucket.StatusCanceled {
			return nil, fmt.Errorf("%v builder failed with %v", name, build.Status)
		}
	}

	return builds, nil
}

func getResults(
	ctx context.Context,
	cfg Config,
	rdb *resultsdb.ResultsDB,
	builds BuildsByName) (result.List, error) {

	results := result.List{}
	err := rdb.QueryTestResults(ctx, builds.ids(), cfg.TestFilter, func(rpb *rdbpb.TestResult) error {
		testName := ""
		status := toStatus(rpb.Status)
		tags := result.NewTags()

		for _, sp := range rpb.Tags {
			switch sp.Key {
			case "test_name":
				testName = sp.Value
			case "typ_tag":
				tags.Add(sp.Value)
			}
		}

		if fr := rpb.GetFailureReason(); fr != nil {
			if strings.Contains(fr.GetPrimaryErrorMessage(), "asyncio.exceptions.TimeoutError") {
				status = result.Slow
			}
		}

		if testName != "" {
			results = append(results, result.Result{
				Query:  query.Parse(testName),
				Status: status,
				Tags:   tags,
			})
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	results.Sort()
	return results, err
}

func loadResults(path string) (result.List, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	results, err := readResults(file)
	if err != nil {
		return nil, fmt.Errorf("while reading '%v': %w", path, err)
	}
	return results, nil
}

func saveResults(path string, results result.List) error {
	dir := filepath.Dir(path)
	if err := os.MkdirAll(dir, 0777); err != nil {
		return err
	}
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()
	return writeResults(file, results)
}

func readResults(r io.Reader) (result.List, error) {
	scanner := bufio.NewScanner(r)
	l := result.List{}
	for scanner.Scan() {
		r, err := result.Parse(scanner.Text())
		if err != nil {
			return nil, err
		}
		l = append(l, r)
	}
	return l, nil
}

func writeResults(w io.Writer, l result.List) error {
	for _, r := range l {
		_, err := fmt.Fprintln(w, r)
		if err != nil {
			return err
		}
	}
	return nil
}

func mergeResults(a, b result.List) result.List {
	merged := make(result.List, 0, len(a)+len(b))
	merged = append(merged, a...)
	merged = append(merged, b...)
	return deduplicate(merged)
}

func deduplicate(l result.List) result.List {
	out := l.ReplaceDuplicates(func(l result.List) result.Status {
		s := l.Statuses()

		// If all results have the same status, then use that
		if len(s) == 1 {
			for status := range s {
				return status
			}
		}

		// Mixed statuses. Replace with something appropriate.
		switch {
		// Crash + * = Crash
		case s.Contains(result.Crash):
			return result.Crash
		// Abort + * = Abort
		case s.Contains(result.Abort):
			return result.Abort
		// Unknown + * = Unknown
		case s.Contains(result.Unknown):
			return result.Unknown
		// RetryOnFailure + ~(Crash | Abort | Unknown) = RetryOnFailure
		case s.Contains(result.RetryOnFailure):
			return result.RetryOnFailure
		// Slow + ~(Crash | Abort | Unknown | RetryOnFailure) = Slow
		case s.Contains(result.Slow):
			return result.Failure
		// Pass + ~(Crash | Abort | Unknown | RetryOnFailure | Slow) = RetryOnFailure
		case s.Contains(result.Pass):
			return result.RetryOnFailure
		}
		return result.Unknown
	})
	out.Sort()
	return out
}

func call(ctx context.Context, exe, wd string, args ...string) (string, error) {
	cmd := exec.CommandContext(ctx, exe, args...)
	out, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}
