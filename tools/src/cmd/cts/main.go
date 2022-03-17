package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"go.chromium.org/luci/auth/client/authcli"
	"go.chromium.org/luci/hardcoded/chromeinfra"
)

type Config struct {
	TestFilter    string
	TestNameRegex string
}

func main() {
	ctx := context.Background()
	if err := run(ctx); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(ctx context.Context) error {
	type verb struct {
		name string
		fn   func(context.Context) error
	}
	verbs := []verb{
		{"roll", roll},
		{"reduce", reduce},
	}
	showUsage := func() error {
		sb := strings.Builder{}
		fmt.Fprintln(&sb, "cts <verb>")
		fmt.Fprintln(&sb)
		fmt.Fprintln(&sb, "<verb> can be one of:")
		for _, verb := range verbs {
			fmt.Fprintln(&sb, "  ", verb.name)
		}
		return errors.New(sb.String())
	}
	if len(os.Args) >= 2 {
		for _, verb := range verbs {
			if verb.name == os.Args[1] {
				return verb.fn(ctx)
			}
		}
	}
	return showUsage()
}

func roll(ctx context.Context) error {
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

	cfg, err := loadConfig(filepath.Join(thisDir(), "config.json"))
	if err != nil {
		return err
	}

	testName, err := regexp.Compile(cfg.TestNameRegex)
	if err != nil {
		return fmt.Errorf("failed to parse test name regex: %w", err)
	}

	err = rdb.QueryTestResults(ctx, "invocations/build-8819539022440269265", cfg.TestFilter, func(result result.Result) error {
		if matches := testName.FindStringSubmatch(result.Name); len(matches) > 1 {
			name := matches[1]
			fmt.Println(result.Status, name, result.Variant.GPU, result.Variant.OS)
		}
		return nil
	})
	if err != nil {
		return err
	}

	return nil
}

func reduceUsage() error {
	sb := strings.Builder{}
	fmt.Sprintln(&sb, "cts reduce <file>")
	return errors.New(sb.String())
}

func reduce(ctx context.Context) error {
	if len(os.Args) < 2 {
		return reduceUsage()
	}
	path := os.Args[2]
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

func thisDir() string {
	_, file, _, ok := runtime.Caller(1)
	if !ok {
		return ""
	}
	return filepath.Dir(file)
}
