// Copyright 2023 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package build

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path"
	"regexp"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cmd/gen/header"
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/glob"
	"dawn.googlesource.com/dawn/tools/src/match"
	"dawn.googlesource.com/dawn/tools/src/template"
	"dawn.googlesource.com/dawn/tools/src/transform"
	"github.com/mzohreva/gographviz/graphviz"
)

const srcTint = "src/tint"

func Generate(verbose, dryRun bool) error {
	p := NewProject(path.Join(fileutils.DawnRoot(), srcTint))
	if file, err := os.Open(path.Join(fileutils.ThisDir(), "externals.json")); err == nil {
		externals := container.Map[string, struct {
			IncludePattern string `json:"include_pattern"`
			Condition      string
		}]{}
		if err := json.NewDecoder(file).Decode(&externals); err != nil {
			return fmt.Errorf("failed to parse 'externals.json': %w", err)
		}
		for _, name := range externals.Keys() {
			external := externals[name]
			match, err := match.New(external.IncludePattern)
			if err != nil {
				return fmt.Errorf("'externals.json' matcher error: %w", err)
			}
			p.externals = append(p.externals, projectExternalDependency{
				name:                name,
				includePatternMatch: match,
				condition:           external.Condition,
			})
		}
	}

	if err := populateSourceFiles(p); err != nil {
		return err
	}
	if err := scanIncludesAndConditions(p); err != nil {
		return err
	}
	if err := loadDirectoryConfigs(p); err != nil {
		return err
	}
	if err := checkForCycles(p); err != nil {
		return err
	}
	if err := emitBuildFiles(p); err != nil {
		return err
	}
	log.Println("emitGraph")
	if err := emitGraph(p); err != nil {
		return err
	}
	return nil
}

func populateSourceFiles(p *Project) error {
	paths, err := glob.Scan(p.Root, glob.MustParseConfig(`{
		"paths": [
			{
				"include": [
					"*/**.cc",
					"*/**.h",
					"*/**.inl"
				]
			},
			{
				"exclude": [
					"fuzzers/**"
				]
			}]
	}`))
	if err != nil {
		return err
	}

	for _, filepath := range paths {
		dir, name := path.Split(filepath)
		if kind := targetKindFromFilename(name); kind != targetInvalid {
			directory := p.AddDirectory(dir)
			p.AddTarget(directory, kind).AddFile(p.AddFile(filepath))
		}
	}

	return nil
}

func scanIncludesAndConditions(p *Project) error {
	type Include struct {
		path string
		line int
	}

	type ParsedFile struct {
		conditions []string
		includes   []Include
	}

	parseFile := func(path string, file *File) (string, *ParsedFile, error) {
		body, err := os.ReadFile(file.AbsPath())
		if err != nil {
			return path, nil, err
		}
		out := &ParsedFile{}
		for i, line := range strings.Split(string(body), "\n") {
			if match := reCondition.FindStringSubmatch(line); len(match) > 0 {
				out.conditions = append(out.conditions, match[1])
			}
			if !reIgnoreInclude.MatchString(line) {
				if match := reInclude.FindStringSubmatch(line); len(match) > 0 {
					out.includes = append(out.includes, Include{match[1], i + 1})
				}
			}
		}
		return path, out, nil
	}

	parsedFiles, err := transform.GoMap(p.Files, parseFile)
	if err != nil {
		return err
	}

	for _, dir := range p.Directories {
		for _, target := range dir.Targets() {
			for _, file := range target.Files() {
				parsed := parsedFiles[file.Path()]
				for _, condition := range parsed.conditions {
					if file.Condition == "" {
						file.Condition = condition
					} else {
						file.Condition += " && " + condition
					}
				}
				for _, include := range parsed.includes {
					if strings.HasPrefix(include.path, srcTint) {
						// #include "src/tint/..."
						include.path = include.path[len(srcTint)+1:] // Strip 'src/tint/'
						includeFile := p.File(include.path)

						if _, err := os.Stat(includeFile.AbsPath()); err != nil {
							return fmt.Errorf(`%v:%v includes non-existent file '%v'`, file.Path(), include.line, includeFile.AbsPath())
						}

						dependencyKind := targetKindFromFilename(include.path)
						if dependencyKind == targetInvalid {
							return fmt.Errorf(`%v:%v unknown target type for include '%v'`, file.Path(), include.line, includeFile.Path())
						}
						if target.Kind == targetLib && dependencyKind == targetTest {
							return fmt.Errorf(`%v:%v lib target must not include test code '%v'`, file.Path(), include.line, includeFile.Path())
						}

						if dependency := p.Target(includeFile.Directory, dependencyKind); dependency != nil {
							target.AddDependency(dependency)
						}
					} else {
						// Check for external includes
						for _, external := range p.externals {
							if external.includePatternMatch(include.path) {
								target.AddExternalDependency(external.name, external.condition)
							}
						}
					}
				}
			}
		}
	}
	return nil
}

func loadDirectoryConfigs(p *Project) error {
	type TargetConfig struct {
		AdditionalDependencies []string
	}
	type DirectoryConfig struct {
		Condition string
		Lib       TargetConfig
		Test      TargetConfig
		Bench     TargetConfig
	}

	for _, dir := range p.Directories.Values() {
		path := path.Join(dir.AbsPath(), "BUILD.cfg")
		if file, err := os.Open(path); err == nil {
			defer file.Close()
			cfg := DirectoryConfig{}
			if err := json.NewDecoder(file).Decode(&cfg); err != nil {
				return fmt.Errorf("error while parsing '%v': %w", path, err)
			}
			for _, target := range dir.Targets() {
				target.Condition = cfg.Condition
			}
			for _, tc := range []struct {
				cfg  *TargetConfig
				kind TargetKind
			}{
				{&cfg.Lib, targetLib},
				{&cfg.Test, targetTest},
				{&cfg.Bench, targetBench},
			} {
				for _, depPattern := range tc.cfg.AdditionalDependencies {
					match, err := match.New(depPattern)
					if err != nil {
						return fmt.Errorf("%v: failed to parse %v.AdditionalDependencies '%v': %w", path, tc.kind, depPattern, err)
					}
					additionalDeps := []*Target{}
					for _, target := range p.Targets.Keys() {
						if match(string(target)) {
							additionalDeps = append(additionalDeps, p.Targets[target])
						}
					}
					if len(additionalDeps) == 0 {
						return fmt.Errorf("%v: %v.AdditionalDependencies '%v' did not match any targets", path, tc.kind, depPattern)
					}
					target := p.AddTarget(dir, tc.kind)
					for _, dep := range additionalDeps {
						target.AddDependency(dep)
					}
				}
			}

		}
	}

	return nil
}

func emitBuildFiles(p *Project) error {
	templatePaths, err := glob.Glob(path.Join(fileutils.ThisDir(), "*.tmpl"))
	if err != nil {
		return err
	}
	if len(templatePaths) == 0 {
		return fmt.Errorf("no template files found")
	}

	templates := container.NewMap[string, *template.Template]()
	for _, path := range templatePaths {
		tmpl, err := template.FromFile(path)
		if err != nil {
			return err
		}
		templates[path] = tmpl
	}

	process := func(dir *Directory) (any, error) {
		for _, tmplPath := range templatePaths {
			_, tmplName := path.Split(tmplPath)
			outputName := strings.TrimSuffix(tmplName, ".tmpl")
			outputPath := path.Join(dir.AbsPath(), outputName)
			existing, err := os.ReadFile(outputPath)
			if err != nil {
				existing = nil
			}
			if reDoNotGenerate.Match(existing) {
				continue
			}
			output, err := os.Create(outputPath)
			if err != nil {
				return nil, err
			}
			defer output.Close()
			output.WriteString(header.Get(string(existing), tmplPath, "#"))
			err = templates[tmplPath].Run(output, dir, map[string]any{})
			if err != nil {
				return nil, err
			}
			output.Close()

			if path.Ext(outputName) == ".gn" {
				if out, err := exec.Command("gn", "format", outputPath).CombinedOutput(); err != nil {
					return nil, fmt.Errorf("gn format failed: %w\n%v", err, string(out))
				}
			}
		}
		return nil, nil
	}

	_, err = transform.GoSlice(p.Directories.Values(), process)
	return err
}

func targetKindFromFilename(filename string) TargetKind {
	switch {
	case strings.HasSuffix(filename, "_test.cc"), strings.HasSuffix(filename, "_test.h"):
		return targetTest
	case strings.HasSuffix(filename, "_bench.cc"), strings.HasSuffix(filename, "_bench.h"):
		return targetBench
	case filename == "main.cc":
		return targetCmd
	case strings.HasSuffix(filename, ".cc"), strings.HasSuffix(filename, ".h"):
		return targetLib
	}
	return targetInvalid
}

func checkForCycles(p *Project) error {
	type state int
	const (
		unvisited state = iota
		visiting
		checked
	)

	cache := container.NewMap[TargetName, state]()

	var walk func(t *Target, path []TargetName) error
	walk = func(t *Target, path []TargetName) error {
		switch cache[t.Name] {
		case unvisited:
			cache[t.Name] = visiting
			for _, dep := range t.Dependencies() {
				if err := walk(dep, append(path, dep.Name)); err != nil {
					return err
				}
			}
			cache[t.Name] = checked
		case visiting:
			err := strings.Builder{}
			fmt.Fprintln(&err, "cyclic target dependency found:")
			for _, t := range path {
				fmt.Fprintln(&err, "  ", string(t))
			}
			fmt.Fprintln(&err, "  ", string(t.Name))
			return fmt.Errorf(err.String())
		}
		return nil
	}

	for _, target := range p.Targets.Values() {
		if err := walk(target, []TargetName{target.Name}); err != nil {
			return err
		}
	}
	return nil
}

func emitGraph(p *Project) error {
	g := graphviz.Graph{}
	nodes := container.NewMap[TargetName, int]()
	targets := []*Target{}
	for _, target := range p.Targets.Values() {
		if target.Kind == "lib" {
			targets = append(targets, target)
		}
	}
	for _, target := range targets {
		nodes.Add(target.Name, g.AddNode(string(target.Name)))
	}
	for _, target := range targets {
		for _, dep := range target.DependencyNames.List() {
			g.AddEdge(nodes[target.Name], nodes[dep], "")
		}
	}

	g.MakeDirected()

	g.DefaultNodeAttribute(graphviz.Shape, graphviz.ShapeBox)
	g.DefaultNodeAttribute(graphviz.FontName, "Courier")
	g.DefaultNodeAttribute(graphviz.FontSize, "14")
	g.DefaultNodeAttribute(graphviz.Style, graphviz.StyleFilled+","+graphviz.StyleRounded)
	g.DefaultNodeAttribute(graphviz.FillColor, "yellow")

	g.DefaultEdgeAttribute(graphviz.FontName, "Courier")
	g.DefaultEdgeAttribute(graphviz.FontSize, "12")

	file, err := os.Create(path.Join(p.Root, "../../docs/tint/targets.dot"))
	if err != nil {
		return err
	}
	defer file.Close()
	g.GenerateDOT(file)
	return nil
}

var (
	reInclude       = regexp.MustCompile(`\s*#\s*include\s*\"([^"]+)\"`)
	reIgnoreInclude = regexp.MustCompile(`//\s*GEN_BUILD:IGNORE_INCLUDE`)
	reCondition     = regexp.MustCompile(`//\s*GEN_BUILD:CONDITION\((.*)\)\s*$`)
	reDoNotGenerate = regexp.MustCompile(`#\s*GEN_BUILD:DO_NOT_GENERATE`)
)
