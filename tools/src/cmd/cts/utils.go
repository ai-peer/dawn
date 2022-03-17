package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"time"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
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

func getBuilds(ctx context.Context, cfg Config, ps gerrit.Patchset, bb *buildbucket.Buildbucket) (BuildsByName, error) {
	// Find any existing builds for the patchset
	builds := BuildsByName{}
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

	// Kick any missing builds
	for name, builder := range cfg.Builders {
		if _, existing := builds[name]; !existing {
			build, err := bb.StartBuild(ctx, ps, builder)
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

func deduplicateResults(l result.List) result.Status {
	switch {
	case l.All(result.Pass):
		return result.Pass
	case l.Any(result.Crash):
		return result.Crash
	case l.Any(result.Abort):
		return result.Abort
	case l.Any(result.Failure):
		return result.Failure
	case l.Any(result.Skip):
		return result.Skip
	default:
		return result.Unknown
	}
}
