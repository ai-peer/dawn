#!/usr/bin/env bash

# Copyright 2019 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Attempts to roll the shaderc related entries in DEPS to origin/master and
# creates a commit.
#
# Depends on roll-dep from depot_path being in PATH.

script_path=$(dirname "$0")
script_path=$(cd "${script_path}" && pwd)
# Assumes script is located one directory before the repo root.
repo_path=$(dirname "${script_path}") 
pwd_path=$(pwd)

glslang_dir="third_party/glslang/"
spirv_cross_dir="third_party/spirv-cross/"
spirv_headers_dir="third_party/spirv-headers/"
spirv_tools_dir="third_party/SPIRV-Tools/"

if [ "${repo_path}" != "${pwd_path}" ]; then
  cd "$repo_path"
fi

roll-dep "${glslang_dir}" "${spirv_cross_dir}" "${spirv_headers_dir}" "${spirv_tools_dir}"

if [ "${repo_path}" != "${pwd_path}" ]; then
  cd "$pwd_path"
fi

