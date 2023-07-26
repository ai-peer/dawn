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
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/transform"
)

type TargetName string

type Target struct {
	Name                  TargetName
	Directory             *Directory
	Kind                  TargetKind
	FilePaths             container.Set[string] // Project relative path
	DependencyNames       container.Set[TargetName]
	ExternalDependencyMap container.Map[string, ExternalDependency]
	Condition             string
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

func (t *Target) ConditionalFiles() []*File {
	return transform.Filter(t.Files(), func(t *File) bool { return t.Condition != "" })
}

func (t *Target) UnconditionalFiles() []*File {
	return transform.Filter(t.Files(), func(t *File) bool { return t.Condition == "" })
}

func (t *Target) AddDependency(dep *Target) {
	if dep != t {
		t.DependencyNames.Add(dep.Name)
	}
}

func (t *Target) AddExternalDependency(name, condition string) {
	t.ExternalDependencyMap.Add(name, ExternalDependency{Name: name, Condition: condition})
}

func (t *Target) Dependencies() []*Target {
	out := make([]*Target, len(t.DependencyNames))
	for i, name := range t.DependencyNames.List() {
		out[i] = t.Directory.Project.Targets[name]
	}
	return out
}

func (t *Target) ConditionalDependencies() []*Target {
	return transform.Filter(t.Dependencies(), func(t *Target) bool { return t.Condition != "" })
}

func (t *Target) UnconditionalDependencies() []*Target {
	return transform.Filter(t.Dependencies(), func(t *Target) bool { return t.Condition == "" })
}

func (t *Target) ExternalDependencies() []ExternalDependency {
	out := make([]ExternalDependency, 0, len(t.ExternalDependencyMap))
	for _, name := range t.ExternalDependencyMap.Keys() {
		out = append(out, t.ExternalDependencyMap[name])
	}
	return out
}

func (t *Target) ConditionalExternalDependencies() []ExternalDependency {
	return transform.Filter(t.ExternalDependencies(), func(t ExternalDependency) bool { return t.Condition != "" })
}

func (t *Target) UnconditionalExternalDependencies() []ExternalDependency {
	return transform.Filter(t.ExternalDependencies(), func(t ExternalDependency) bool { return t.Condition == "" })
}
