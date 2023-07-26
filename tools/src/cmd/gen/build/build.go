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
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cmd/gen/header"
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/glob"
	"dawn.googlesource.com/dawn/tools/src/template"
	"github.com/mzohreva/gographviz/graphviz"
)

const srcTint = "src/tint"

type TargetKind string

const (
	targetLib     TargetKind = "lib"
	targetTest    TargetKind = "test"
	targetBench   TargetKind = "bench"
	targetCmd     TargetKind = "cmd"
	targetInvalid TargetKind = "<invalid>"
)

type File struct {
	Directory *Directory
	Name      string
	Condition string
}

func (f *File) Path() string {
	return filepath.Join(f.Directory.Path, f.Name)
}

func (f *File) AbsPath() string {
	return filepath.Join(f.Directory.AbsPath(), f.Name)
}

type TargetName string
type Target struct {
	Name            TargetName
	Directory       *Directory
	Kind            TargetKind
	FilePaths       container.Set[string] // Project relative path
	DependencyNames container.Set[TargetName]
	Config          map[string]any
}

func (t *Target) AddFile(f *File) {
	t.FilePaths.Add(f.Path())
}

func (t *Target) Files() []*File {
	out := make([]*File, len(t.FilePaths))
	for i, name := range t.FilePaths.List() {
		out[i] = t.Directory.Project.Files[name]
	}
	return out
}

func (t *Target) AddDependency(dep *Target) {
	t.DependencyNames.Add(dep.Name)
}

func (t *Target) Dependencies() []*Target {
	out := make([]*Target, len(t.DependencyNames))
	for i, name := range t.DependencyNames.List() {
		out[i] = t.Directory.Project.Targets[name]
	}
	return out
}

type Directory struct {
	Project     *Project
	Path        string
	TargetNames container.Set[TargetName]
}

func (d *Directory) AbsPath() string {
	return filepath.Join(d.Project.Root, d.Path)
}

func (d *Directory) Name() string {
	_, name := filepath.Split(d.Path)
	return name
}

func (d *Directory) Depth() int {
	return strings.Count(d.Path, "/")
}

func (d *Directory) Targets() []*Target {
	out := make([]*Target, len(d.TargetNames))
	for i, name := range d.TargetNames.List() {
		out[i] = d.Project.Targets[name]
	}
	return out
}

type Project struct {
	Root        string
	Files       container.Map[string, *File]
	Directories container.Map[string, *Directory]
	Targets     container.Map[TargetName, *Target]
}

func NewProject(root string) *Project {
	return &Project{
		Root:        root,
		Files:       container.NewMap[string, *File](),
		Directories: container.NewMap[string, *Directory](),
		Targets:     container.NewMap[TargetName, *Target](),
	}
}

func (p *Project) File(path string) *File {
	dir, name := filepath.Split(path)
	f := p.Files[path]
	if f == nil {
		f = &File{
			Directory: p.Directory(dir),
			Name:      name,
		}
		p.Files[path] = f
	}
	return f
}

func (p *Project) Target(dir *Directory, kind TargetKind) *Target {
	name := TargetName(dir.Path)
	if kind != targetLib {
		name += TargetName(fmt.Sprintf(":%v", kind))
	}

	t := p.Targets[name]
	if t == nil {
		t = &Target{
			Name:            name,
			Directory:       dir,
			Kind:            kind,
			FilePaths:       container.NewSet[string](),
			DependencyNames: container.NewSet[TargetName](),
		}
		dir.TargetNames.Add(name)
		p.Targets.Add(name, t)
	}
	return t
}

func (p *Project) Directory(path string) *Directory {
	path = strings.TrimSuffix(path, string(filepath.Separator))
	d := p.Directories[path]
	if d == nil {
		d = &Directory{
			Project:     p,
			Path:        path,
			TargetNames: container.NewSet[TargetName](),
		}
		p.Directories[path] = d
	}
	return d
}

func Generate(verbose, dryRun bool) error {
	p := NewProject(fileutils.DawnRoot())
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
					"src/tint/**.cc",
					"src/tint/**.h",
					"src/tint/**.inl"
				]
			},
			{
				"exclude": [
					"src/tint/fuzzers/**"
				]
			}]
	}`))
	if err != nil {
		return err
	}

	for _, path := range paths {
		dir, name := filepath.Split(path)
		directory := p.Directory(dir)
		if kind := targetKindFromFilename(name); kind != targetInvalid {
			p.Target(directory, kind).AddFile(p.File(path))
		}
	}

	return nil
}

func scanIncludesAndConditions(p *Project) error {
	for _, dir := range p.Directories {
		for _, target := range dir.Targets() {
			for _, file := range target.Files() {
				body, err := ioutil.ReadFile(file.AbsPath())
				if err != nil {
					return err
				}
				for i, line := range strings.Split(string(body), "\n") {
					// Scan for GEN_BUILD directive
					if match := reCondition.FindStringSubmatch(line); len(match) > 0 {
						file.Condition = match[1]
					}
					// Scan for #include
					if match := reInclude.FindStringSubmatch(line); len(match) > 0 {
						includePath := match[1]
						if !strings.HasPrefix(includePath, srcTint) {
							continue
						}
						includeFile := p.File(includePath)

						if !reIgnoreInclude.MatchString(line) {
							if _, err := os.Stat(includeFile.AbsPath()); err != nil {
								return fmt.Errorf(`%v:%v includes non-existent file '%v'`, file.Path(), i+1, includeFile.Path())
							}
						}

						dependencyKind := targetKindFromFilename(includePath)
						if dependencyKind == targetInvalid {
							return fmt.Errorf(`%v:%v unknown target type for include '%v'`, file.Path(), i+1, includeFile.Path())
						}
						if target.Kind == targetLib && dependencyKind == targetTest {
							return fmt.Errorf(`%v:%v lib target must not include test code '%v'`, file.Path(), i+1, includeFile.Path())
						}

						dependency := p.Target(includeFile.Directory, dependencyKind)
						if dependency != target {
							target.AddDependency(dependency)
						}
					}
				}
			}
		}
	}
	return nil
}

func loadDirectoryConfigs(p *Project) error {
	type DirectoryConfig map[TargetKind]map[string]any

	for _, dir := range p.Directories {
		path := filepath.Join(dir.AbsPath(), "BUILD.cfg")
		if file, err := os.Open(path); err == nil {
			defer file.Close()
			cfg := DirectoryConfig{}
			if err := json.NewDecoder(file).Decode(&cfg); err != nil {
				return fmt.Errorf("error while parsing '%v': %w", path, err)
			}
			for _, target := range dir.Targets() {
				target.Config = cfg[target.Kind]
			}
		}
	}

	return nil
}

func emitBuildFiles(p *Project) error {
	templatePaths, err := glob.Glob(filepath.Join(fileutils.ThisDir(), "*.tmpl"))
	if err != nil {
		return err
	}
	if len(templatePaths) == 0 {
		return fmt.Errorf("no template files found")
	}

	// Path to template content
	templates := container.NewMap[string, string]()
	for _, path := range templatePaths {
		content, err := ioutil.ReadFile(path)
		if err != nil {
			return err
		}
		templates.Add(path, string(content))
	}

	for _, dir := range p.Directories.Values() {
		for _, tmplPath := range templatePaths {
			_, tmplName := filepath.Split(tmplPath)
			outputName := strings.TrimSuffix(tmplName, ".tmpl")
			outputPath := filepath.Join(dir.AbsPath(), outputName)
			existing, err := ioutil.ReadFile(outputPath)
			if err != nil {
				existing = nil
			}
			if reDoNotGenerate.Match(existing) {
				continue
			}
			output, err := os.Create(outputPath)
			if err != nil {
				return err
			}
			defer output.Close()
			output.WriteString(header.Get(string(existing), tmplPath, "#"))
			err = template.Run(templates[tmplPath], output, dir, map[string]any{})
			if err != nil {
				return err
			}
			output.Close()

			if out, err := exec.Command("gn", "format", outputPath).CombinedOutput(); err != nil {
				return fmt.Errorf("gn format failed: %w\n%v", err, string(out))
			}
		}
	}
	return nil
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
	// Populate roots with all the target names
	roots := container.NewSet(p.Targets.Keys()...)

	// Remove everything that's used as a dependency, leaving just the roots
	for _, target := range p.Targets.Values() {
		for dep := range target.DependencyNames {
			roots.Remove(dep)
		}
	}

	// Transform roots into a list of target paths
	paths := [][]TargetName{}
	for _, name := range roots.List() {
		paths = append(paths, []TargetName{name})
	}
	for len(paths) > 0 {
		// Pop the last path off paths
		path := paths[len(paths)-1]
		paths = paths[:len(paths)-1]

		// Get the current target from 'path'
		curr := p.Targets[path[len(path)-1]]
		for _, dep := range curr.Dependencies() {
			if container.NewSet(path...).Contains(dep.Name) {
				err := strings.Builder{}
				fmt.Fprintln(&err, "cyclic target dependency found:")
				for _, t := range path {
					fmt.Fprintln(&err, "  ", string(t))
				}
				fmt.Fprintln(&err, "  ", string(dep.Name))
				return fmt.Errorf(err.String())
			}
			depPath := make([]TargetName, len(path)+1)
			copy(depPath, path)
			depPath[len(path)] = dep.Name
			paths = append(paths, depPath)
		}
	}
	return nil
}

func emitGraph(p *Project) error {
	g := graphviz.Graph{}
	nodes := container.NewMap[TargetName, int]()
	targets := []*Target{}
	for _, target := range p.Targets.Values() {
		if target.Kind == "lib" && strings.HasPrefix(string(target.Name), "src/tint/utils") {
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

	file, err := os.Create(filepath.Join(p.Root, "docs/tint/targets.dot"))
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
	reCondition     = regexp.MustCompile(`//\s*GEN_BUILD:CONDITION\(([^\(]*)\)`)
	reDoNotGenerate = regexp.MustCompile(`#\s*GEN_BUILD:DO_NOT_GENERATE`)
)
