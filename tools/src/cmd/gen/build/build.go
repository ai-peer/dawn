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
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"regexp"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cmd/gen/header"
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/fileutils"
	"dawn.googlesource.com/dawn/tools/src/glob"
	"dawn.googlesource.com/dawn/tools/src/template"
)

const srcTint = "src/tint"

type TargetKind string

const (
	lib   TargetKind = "lib"
	test  TargetKind = "test"
	bench TargetKind = "bench"
)

type File struct {
	Directory *Directory
	Name      string
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
	Files           container.Set[string]
	DependencyNames container.Set[TargetName]
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
	}
	return f
}

func (p *Project) Target(dir *Directory, kind TargetKind) *Target {
	name := TargetName(fmt.Sprintf("%v:%v", dir.Path, kind))
	t := p.Targets[name]
	if t == nil {
		t = &Target{
			Name:            name,
			Directory:       dir,
			Kind:            kind,
			Files:           container.NewSet[string](),
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
	if err := scanIncludes(p); err != nil {
		return err
	}
	if err := emitBuildFiles(p); err != nil {
		return err
	}
	return nil
}

func populateSourceFiles(p *Project) error {
	return filepath.Walk(filepath.Join(p.Root, srcTint), func(path string, info os.FileInfo, err error) error {
		path, err = filepath.Rel(p.Root, path) // abs -> rel
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}
		dir, name := filepath.Split(path)
		directory := p.Directory(dir)
		var target *Target
		switch {
		case strings.HasSuffix(name, "_test.cc"), strings.HasSuffix(name, "_test.h"):
			target = p.Target(directory, test)
		case strings.HasSuffix(name, "_bench.cc"), strings.HasSuffix(name, "_bench.h"):
			target = p.Target(directory, bench)
		case strings.HasSuffix(name, ".cc"), strings.HasSuffix(name, ".h"):
			target = p.Target(directory, lib)
		}
		if target != nil {
			target.Files.Add(path)
		}
		return nil
	})
}

func scanIncludes(p *Project) error {
	for _, dir := range p.Directories {
		for _, target := range dir.Targets() {
			for filePath := range target.Files {
				file := p.File(filePath)
				body, err := ioutil.ReadFile(file.AbsPath())
				if err != nil {
					return err
				}
				for i, line := range strings.Split(string(body), "\n") {
					match := reInclude.FindStringSubmatch(line)
					if len(match) == 0 {
						continue
					}
					includePath := match[1]
					if !strings.HasPrefix(includePath, srcTint) {
						continue
					}
					includeFile := p.File(includePath)
					if !strings.Contains(line, "// GENERATED") {
						if _, err := os.Stat(includeFile.AbsPath()); err != nil {
							return fmt.Errorf(`%v:%v includes non-existent file '%v'`, file.Path(), i+1, includeFile.Path())
						}
					}
					dependency := p.Target(includeFile.Directory, target.Kind)
					if dependency != target {
						target.DependencyNames.Add(dependency.Name)
					}
				}
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
		fmt.Println(dir.Path)
		for _, tmplPath := range templatePaths {
			_, tmplName := filepath.Split(tmplPath)
			outputName := strings.TrimSuffix(tmplName, ".tmpl")
			outputPath := filepath.Join(dir.AbsPath(), outputName)
			existing, err := ioutil.ReadFile(outputPath)
			if err != nil {
				existing = nil
			}
			if strings.Contains(string(existing), "DO_NOT_GENERATE") {
				continue
			}
			output, err := os.Create(outputPath)
			if err != nil {
				return err
			}
			output.WriteString(header.Get(string(existing), tmplPath, "#"))
			err = template.Run(templates[tmplPath], output, dir, map[string]any{})
			if err != nil {
				return err
			}
		}
	}
	return nil
}

var reInclude = regexp.MustCompile(`\s*#\s*include\s*\"([^"]+)\"`)
