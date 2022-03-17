package main

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
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
	"dawn.googlesource.com/dawn/tools/src/cts/query"
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
	Expectations expectations.Config
	Builders     map[string]buildbucket.Builder
}

var errInvalidCLA = errors.New("invalid command line args")

func main() {
	ctx := context.Background()
	if err := run(ctx); err != nil {
		if err != errInvalidCLA {
			fmt.Fprintln(os.Stderr, err)
		}
		os.Exit(1)
	}
}

func invalidCLA() error {
	flag.Usage()
	return errInvalidCLA
}

type Command interface {
	name() string
	desc() string
	registerFlags(ctx context.Context, cfg Config) ([]string, error)
	run(ctx context.Context, cfg Config) error
}

func run(ctx context.Context) error {
	cfg, err := loadConfig(filepath.Join(thisDir(), "config.json"))
	if err != nil {
		return err
	}

	cmds := []Command{
		&cmdRoll{}, &cmdResults{}, &cmdFormat{}, &cmdUpdate{}, &cmdReduce{},
	}

	flag.Usage = func() {
		out := flag.CommandLine.Output()
		fmt.Fprintln(out, "cts [command]")
		fmt.Fprintln(out)
		fmt.Fprintln(out, "Commands:")
		for _, cmd := range cmds {
			fmt.Fprintln(out, "  ", cmd.name())
		}
		flag.PrintDefaults()
	}

	if len(os.Args) >= 2 {
		help := os.Args[1] == "help"
		if help {
			copy(os.Args[1:], os.Args[2:])
			os.Args = os.Args[:len(os.Args)-1]
		}

		for _, cmd := range cmds {
			if cmd.name() == os.Args[1] {
				out := flag.CommandLine.Output()
				args, err := cmd.registerFlags(ctx, *cfg)
				if err != nil {
					return err
				}
				flag.Usage = func() {
					hasFlags := false
					flag.VisitAll(func(*flag.Flag) { hasFlags = true })
					flagsAndArgs := args
					if hasFlags {
						flagsAndArgs = append(append([]string{}, args...), "<flags>")
					}
					fmt.Fprintln(out, "cts", cmd.name(), strings.Join(flagsAndArgs, " "))
					fmt.Fprintln(out)
					fmt.Fprintln(out, cmd.desc())
					if hasFlags {
						fmt.Fprintln(out)
						fmt.Fprintln(out, "flags:")
						flag.PrintDefaults()
					}
				}
				if help {
					flag.Usage()
					return nil
				}
				if len(os.Args) < len(args)+2 {
					fmt.Fprintln(out, "missing argument", args[len(os.Args)-2])
					fmt.Fprintln(out)
					return invalidCLA()
				}
				if err := flag.CommandLine.Parse(os.Args[2:]); err != nil {
					return err
				}
				return cmd.run(ctx, *cfg)
			}
		}
	}

	return invalidCLA()
}

type cmdRoll struct{}

func (cmdRoll) name() string { return "roll" }
func (cmdRoll) desc() string { return "roll CTS and re-generate expectations" }
func (cmdRoll) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	return nil, nil
}
func (c *cmdRoll) run(ctx context.Context, cfg Config) error {
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

////////////////////////////////////////////////////////////////////////////////
// cts results
////////////////////////////////////////////////////////////////////////////////
type cmdResults struct {
	flags struct {
		output string
		ps     gerrit.Patchset
		auth   authcli.Flags
	}
}

func (cmdResults) name() string { return "results" }
func (cmdResults) desc() string { return "obtains the CTS results for a patchset" }
func (c *cmdResults) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.output, "o", "", "output file. If unspecified writes to stdout")
	c.flags.ps.RegisterFlags(cfg.Gerrit.Defaults.Host, cfg.Gerrit.Defaults.Project)
	c.flags.auth.Register(flag.CommandLine, chromeinfra.DefaultAuthOptions())
	return nil, nil
}
func (c *cmdResults) run(ctx context.Context, cfg Config) error {
	// Compile testName regex
	testName, err := regexp.Compile(cfg.TestNameRegex)
	if err != nil {
		return fmt.Errorf("failed to parse test name regex: %w", err)
	}

	// Validate command line arguments
	opts, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	if c.flags.ps.Change == 0 {
		flag.Usage()
		return fmt.Errorf("missing --change flag")
	}
	if c.flags.ps.Patchset == 0 {
		flag.Usage()
		return fmt.Errorf("missing --ps flag")
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

	// Obtain CTS results
	bb, err := buildbucket.New(ctx, opts)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, opts)
	if err != nil {
		return err
	}

	builds, err := getBuilds(ctx, cfg, c.flags.ps, bb)
	if err != nil {
		return err
	}

	results := result.List{}
	err = rdb.QueryTestResults(ctx, builds.ids(), cfg.TestFilter, func(rpb *rdbpb.TestResult) error {
		if matches := testName.FindStringSubmatch(rpb.TestId); len(matches) > 1 {
			defs := rpb.GetVariant().GetDef()
			query, err := query.Parse(matches[1])
			if err != nil {
				return err
			}
			results = append(results, result.Result{
				Query:  query,
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

	return writeResults(output, results)
}

////////////////////////////////////////////////////////////////////////////////
// cts update
////////////////////////////////////////////////////////////////////////////////
type cmdUpdate struct {
	flags struct {
		results      string
		expectations string
	}
}

func (cmdUpdate) name() string { return "update" }
func (cmdUpdate) desc() string { return "updates a CTS expectations file" }
func (c *cmdUpdate) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.results, "results", "results.txt", "path to CTS results file")
	flag.StringVar(&c.flags.expectations, "expectations", "expectations.txt", "path to CTS expectations file to update")
	return nil, nil
}
func (c *cmdUpdate) run(ctx context.Context, cfg Config) error {
	resultsFile, err := os.Open(c.flags.results)
	if err != nil {
		return err
	}
	defer resultsFile.Close()

	results, err := readResults(resultsFile)
	if err != nil {
		return err
	}

	// Collapse all duplicate results
	results = results.ReplaceDuplicates(deduplicateResults)

	ex, err := expectations.Load(c.flags.expectations)
	if err != nil {
		return err
	}

	msgs, err := ex.Update(results, cfg.Expectations)
	if err != nil {
		return err
	}
	for _, msg := range msgs {
		fmt.Println(msg)
	}

	return ex.Save(c.flags.expectations)
}

////////////////////////////////////////////////////////////////////////////////
// cts format
////////////////////////////////////////////////////////////////////////////////
type cmdFormat struct {
	flags struct {
	}
}

func (cmdFormat) name() string { return "format" }
func (cmdFormat) desc() string { return "formats a WebGPUExpectation file" }
func (c *cmdFormat) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	return []string{"<file>"}, nil
}
func (c *cmdFormat) run(ctx context.Context, cfg Config) error {
	path := flag.Args()[0]
	ex, err := expectations.Load(path)
	if err != nil {
		return err
	}
	return ex.Save(path)
}

////////////////////////////////////////////////////////////////////////////////
// cts reduce
////////////////////////////////////////////////////////////////////////////////
type cmdReduce struct {
	flags struct {
		output string
	}
}

func (cmdReduce) name() string { return "reduce" }
func (cmdReduce) desc() string { return `simplify results by collapsing common results` }
func (c *cmdReduce) registerFlags(ctx context.Context, cfg Config) ([]string, error) {
	flag.StringVar(&c.flags.output, "o", "", "output file. If unspecified writes to stdout")
	return []string{"<file>"}, nil
}
func (c *cmdReduce) run(ctx context.Context, cfg Config) error {
	path := flag.Arg(0)
	input, err := os.Open(path)
	if err != nil {
		return fmt.Errorf("failed to open '%v': %w", path, err)
	}
	defer input.Close()

	results, err := readResults(input)
	if err != nil {
		return err
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

	// Collapse all duplicate results
	results = results.ReplaceDuplicates(deduplicateResults)

	// Build a tree of the results
	tree, err := tree.New(results)
	if err != nil {
		return err
	}

	// Reduce the tree
	tree.Reduce()

	results = tree.AllResults()

	return writeResults(output, results)
}

////////////////////////////////////////////////////////////////////////////////
// utils
////////////////////////////////////////////////////////////////////////////////
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
	case l.Any(result.Fail):
		return result.Fail
	case l.Any(result.Skip):
		return result.Skip
	default:
		return result.Unknown
	}
}
