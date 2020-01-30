// Copyright 2017 The Dawn Authors
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

#ifndef COMMON_VULKANPLATFORM_H_
#define COMMON_VULKANPLATFORM_H_

#if !defined(DAWN_ENABLE_BACKEND_VULKAN)
#    error "vulkan_platform.h included without the Vulkan backend enabled"
#endif

#include "common/Platform.h"

#include <vulkan/vulkan.h>

// Redefine VK_NULL_HANDLE for better type safety where possible.
#undef VK_NULL_HANDLE
#if defined(DAWN_PLATFORM_64_BIT)
static constexpr nullptr_t VK_NULL_HANDLE = nullptr;
#elif defined(DAWN_PLATFORM_32_BIT)
static constexpr uint64_t VK_NULL_HANDLE = 0;
#else
#    error "Unsupported platform"
#endif

// Remove windows.h macros after vulkan_platform's include of windows.h
#if defined(DAWN_PLATFORM_WINDOWS)
#    include "common/windows_with_undefs.h"
#endif
// Remove X11/Xlib.h macros after vulkan_platform's include of it.
#if defined(DAWN_USE_X11)
#    include "common/xlib_with_undefs.h"
#endif

// Include Fuchsia-specific definitions that are not upstreamed yet.
#if defined(DAWN_PLATFORM_FUCHSIA)
#    include <vulkan/vulkan_fuchsia_extras.h>
#endif

#endif  // COMMON_VULKANPLATFORM_H_
