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
	"path"
	"path/filepath"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/match"
)

// Project holds information about all the directories, targets and source files
// that makes up a project.
type Project struct {
	// The absolute path to the root of the project
	Root string
	// A map of project-relative path to File.
	Files container.Map[string, *File]
	// A map of project-relative path to Directory.
	Directories container.Map[string, *Directory]
	// A map of target name to target.
	Targets container.Map[TargetName, *Target]
	// A list of external project dependencies used by the project
	externals []projectExternalDependency
}

// projectExternalDependency describes an external dependency of a Project
type projectExternalDependency struct {
	// name of the external dependency
	name ExternalDependencyName
	// include matcher
	includePatternMatch match.Test
	// condition of the dependency
	condition string
}

// NewProject returns a newly initialized Project
func NewProject(root string) *Project {
	return &Project{
		Root:        root,
		Files:       container.NewMap[string, *File](),
		Directories: container.NewMap[string, *Directory](),
		Targets:     container.NewMap[TargetName, *Target](),
	}
}

// AddFile gets or creates a File with the given project-relative path
func (p *Project) AddFile(file string) *File {
	return p.Files.GetOrCreate(file, func() *File {
		dir, name := path.Split(file)
		return &File{
			Directory: p.Directory(dir),
			Name:      name,
		}
	})
}

// File returns the File with the given project-relative path
func (p *Project) File(file string) *File {
	return p.Files[file]
}

// AddTarget gets or creates a Target with the given Directory and TargetKind
func (p *Project) AddTarget(dir *Directory, kind TargetKind) *Target {
	name := p.TargetName(dir, kind)
	return p.Targets.GetOrCreate(name, func() *Target {
		t := &Target{
			Name:                  name,
			Directory:             dir,
			Kind:                  kind,
			SourceFileSet:         container.NewSet[string](),
			DependencyNames:       container.NewSet[TargetName](),
			ExternalDependencyMap: container.NewMap[ExternalDependencyName, ExternalDependency](),
		}
		dir.TargetNames.Add(name)
		p.Targets.Add(name, t)
		return t
	})
}

// Target returns the Target with the given Directory and TargetKind
func (p *Project) Target(dir *Directory, kind TargetKind) *Target {
	return p.Targets[p.TargetName(dir, kind)]
}

// TargetName returns the TargetName of a target in dir with the given kind
func (p *Project) TargetName(dir *Directory, kind TargetKind) TargetName {
	name := TargetName(dir.Path)
	if kind != targetLib {
		name += TargetName(fmt.Sprintf(":%v", kind))
	}
	return name
}

// AddDirectory gets or creates a Directory with the given project-relative path
func (p *Project) AddDirectory(path string) *Directory {
	path = p.CanonicalizePath(path)
	return p.Directories.GetOrCreate(path, func() *Directory {
		split := strings.Split(path, "/")
		d := &Directory{
			Project:           p,
			Name:              split[len(split)-1],
			Path:              path,
			TargetNames:       container.NewSet[TargetName](),
			SubdirectoryNames: container.NewSet[string](),
		}
		p.Directories[path] = d

		if path != "" {
			d.Parent = p.AddDirectory(strings.Join(split[:len(split)-1], "/"))
			d.Parent.SubdirectoryNames.Add(d.Name)
		}
		return d
	})
}

// Directory returns the Directory with the given project-relative path
func (p *Project) Directory(path string) *Directory {
	return p.Directories[p.CanonicalizePath(path)]
}

// CanonicalizePath canonicalizes the given path by changing path delimiters to
// '/' and removing any trailing slash
func (p *Project) CanonicalizePath(path string) string {
	return strings.TrimSuffix(filepath.ToSlash(path), "/")
}
