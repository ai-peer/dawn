// Copyright 2022 The Dawn Authors
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

#ifndef SRC_DAWN_UTILS_WEBGPUHEADERINCLUDE_H_
#define SRC_DAWN_UTILS_WEBGPUHEADERINCLUDE_H_

// Instead of directly doing include "dawn/webgpu_cpp.h"
// dawn test files do include "/path/to/WebGPUHeaderInclude.h"
// to support build by emscripten or native at the same time.

#ifdef __EMSCRIPTEN__
#    include <webgpu/webgpu_cpp.h>
#else
#    include "dawn/webgpu_cpp.h"
#endif

#endif  // SRC_DAWN_UTILS_WEBGPUHEADERINCLUDE_H_
