// Copyright 2020 The Dawn Authors
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

#ifndef UTILS_GLFWUTILS_H_
#define UTILS_GLFWUTILS_H_

#include "dawn/webgpu_cpp.h"

#include <memory>

struct GLFWwindow;

namespace utils {

    void SetupGLFWWindowHintsForBackend(wgpu::BackendType type);

    wgpu::Surface CreateSurfaceForWindow(wgpu::Instance instance, GLFWwindow* window);

    std::unique_ptr<wgpu::ChainedStruct> SetupWindowAndGetSurfaceDescriptorForTesting(
        GLFWwindow* window);

}  // namespace utils

#endif  // UTILS_GLFWUTILS_H_
