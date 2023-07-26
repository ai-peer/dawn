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
	"path"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/container"
)

type Directory struct {
	Project           *Project
	Parent            *Directory
	Name              string
	Path              string
	SubdirectoryNames container.Set[string]
	TargetNames       container.Set[TargetName]
}

func (d *Directory) AbsPath() string {
	return path.Join(d.Project.Root, d.Path)
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

func (d *Directory) Subdirectories() []*Directory {
	out := make([]*Directory, len(d.SubdirectoryNames))
	for i, name := range d.SubdirectoryNames.List() {
		out[i] = d.Project.Directories[path.Join(d.Path, name)]
	}
	return out
}
