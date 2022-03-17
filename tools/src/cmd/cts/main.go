package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
)

type Config struct {
	TestFilter    string
	TestNameRegex string
	Gerrit        struct {
		Defaults struct {
			Host    string
			Project string
		}
	}
	Builders map[string]buildbucket.Builder
}

func main() {
	ctx := context.Background()
	if err := run(ctx); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(ctx context.Context) error {
	cfg, err := loadConfig(filepath.Join(thisDir(), "config.json"))
	if err != nil {
		return err
	}

	type Command struct {
		name string
		fn   func(context.Context, Config) error
		desc string
	}
	cmds := []Command{
		{"roll", roll, "roll CTS and re-generate expectations"},
		{"status", status, "runs CTS on a patchset and emits the status of each test"},
		{"format", format, "formats a WebGPUExpectations file"},
		{"reduce", reduce, "simplify a status file by collapsing common results"},
	}
	showUsage := func() error {
		sb := strings.Builder{}
		fmt.Fprintln(&sb, "cts [command]")
		fmt.Fprintln(&sb)
		fmt.Fprintln(&sb, "Commands:")
		for _, cmd := range cmds {
			fmt.Fprintln(&sb, "  ", cmd.name)
		}
		return errors.New(sb.String())
	}
	if len(os.Args) >= 2 {
		for _, cmd := range cmds {
			if cmd.name == os.Args[1] {
				return cmd.fn(ctx, *cfg)
			}
		}
	}
	return showUsage()
}

func roll(ctx context.Context, cfg Config) error {
	/*
		authFlags := authcli.Flags{}
		authFlags.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
		flag.Parse()

		opts, err := authFlags.Options()
		if err != nil {
			return fmt.Errorf("failed to obtain authentication options: %w", err)
		}

		rdb, err := resultsdb.New(ctx, opts)
		if err != nil {
			return err
		}

		testName, err := regexp.Compile(cfg.TestNameRegex)
		if err != nil {
			return fmt.Errorf("failed to parse test name regex: %w", err)
		}

		err = rdb.QueryTestResults(ctx, 8819539022440269265, cfg.TestFilter, func(result result.Result) error {
			if matches := testName.FindStringSubmatch(result.Name); len(matches) > 1 {
				name := matches[1]
				fmt.Println(result.Status, name, result.Variant.GPU, result.Variant.OS)
			}
			return nil
		})
		if err != nil {
			return err
		}
	*/
	panic("TODO")
}

func status(ctx context.Context, cfg Config) error {
	// Compile testName regex
	testName, err := regexp.Compile(cfg.TestNameRegex)
	if err != nil {
		return fmt.Errorf("failed to parse test name regex: %w", err)
	}

	// Process verb command line flags
	ps := gerrit.Patchset{}
	ps.RegisterFlags(cfg.Gerrit.Defaults.Host, cfg.Gerrit.Defaults.Project)

	authFlags := authcli.Flags{}
	authFlags.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	flag.CommandLine.Parse(os.Args[2:])

	opts, err := authFlags.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	// Validate command line arguments
	if ps.Change == 0 {
		flag.Usage()
		return fmt.Errorf("missing --change flag")
	}
	if ps.Patchset == 0 {
		flag.Usage()
		return fmt.Errorf("missing --ps flag")
	}

	bb, err := buildbucket.New(ctx, opts)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, opts)
	if err != nil {
		return err
	}

	builds, err := getBuilds(ctx, cfg, ps, bb)
	if err != nil {
		return err
	}

	results := result.List{}
	err = rdb.QueryTestResults(ctx, builds.ids(), cfg.TestFilter, func(rpb *rdbpb.TestResult) error {
		if matches := testName.FindStringSubmatch(rpb.TestId); len(matches) > 1 {
			defs := rpb.GetVariant().GetDef()
			results = append(results, result.Result{
				Name:   matches[1],
				Status: toStatus(rpb.Status),
				Variant: result.Variant{
					// Flags: defs["test_suite"],
					GPU: result.GPU(defs["gpu"]),
					OS:  result.OS(defs["os"]),
				},
			})
		}
		return nil
	})
	if err != nil {
		return err
	}

	results.Sort()

	for _, result := range results {
		fmt.Println(result.Status, result.Name, result.Variant.GPU, result.Variant.OS, result.Variant.Flags)
	}

	return nil
}

func formatUsage() error {
	sb := strings.Builder{}
	fmt.Fprintln(&sb, "cts format <file>")
	fmt.Fprintln(&sb)
	fmt.Fprintln(&sb, "Formats a WebGPUExpectation file")
	return errors.New(sb.String())
}

func format(ctx context.Context, cfg Config) error {
	if len(os.Args) < 2 {
		return formatUsage()
	}
	path := os.Args[2]
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return err
	}
	ex, err := expectations.Parse(path, string(content))
	if err != nil {
		return err
	}
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	return ex.Write(f)
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
		if build.Status != buildbucket.StatusSuccess {
			return nil, fmt.Errorf("%v builder failed with %v", name, build.Status)
		}
	}

	return builds, nil
}

func reduceUsage() error {
	sb := strings.Builder{}
	fmt.Fprintln(&sb, "cts reduce <file>")
	return errors.New(sb.String())
}

func reduce(ctx context.Context, cfg Config) error {
	if len(os.Args) < 2 {
		return reduceUsage()
	}
	flag.CommandLine.Parse(os.Args[2:])
	path := flag.Arg(0)
	bytes, err := os.ReadFile(path)
	if err != nil {
		return err
	}

	lines := strings.Split(string(bytes), "\n")
	results := make(result.List, 0, len(lines))
	for i, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		parts := strings.Split(line, " ")
		if len(parts) != 4 {
			return fmt.Errorf("%v:%v failed to parse line", path, i+1)
		}
		results = append(results, result.Result{
			Status: result.Status(parts[0]),
			Name:   parts[1],
			Variant: result.Variant{
				GPU: result.GPU(parts[2]),
				OS:  result.OS(parts[3]),
			},
		})
	}

	results = results.ReplaceDuplicates(func(l result.List) result.Status {
		for _, r := range l {
			if r.Status != result.Pass {
				return result.Fail
			}
		}
		return result.Pass
	})

	tree, err := tree.New(results)
	if err != nil {
		return err
	}

	tree.Reduce()
	for _, result := range tree.List() {
		fmt.Println(result.Status, result.Name, result.Variant.GPU, result.Variant.OS)
	}

	return nil
}

func loadConfig(path string) (*Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
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
		return result.Fail
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
