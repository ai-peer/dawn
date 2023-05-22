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

#ifndef SRC_DAWN_NATIVE_VULKAN_EXTERNALHANDLE_H_
#define SRC_DAWN_NATIVE_VULKAN_EXTERNALHANDLE_H_

#include "dawn/common/ityp_array.h"
#include "dawn/common/vulkan_platform.h"

namespace dawn::native::vulkan {

using ExternalSemaphoreHandle = int;
constexpr ityp::
    array<wgpu::DawnVkSemaphoreType, ExternalSemaphoreHandle, kEnumCount<wgpu::DawnVkSemaphoreType>>
        kInvalidExternalSemaphoreHandle = {
            /* fd */ -1,
            /* fd */ -1,
            /* ZX_HANDLE_INVALID */ 0,
};
#if DAWN_PLATFORM_IS(FUCHSIA)
static_assert(ZX_HANDLE_INVALID == 0);
#endif

// Static assert the handle types because we've hard coded the invalid handle values based on what
// the type is.
static_assert(static_cast<uint32_t>(wgpu::DawnVkSemaphoreType::OpaqueFD) == 0u);
static_assert(static_cast<uint32_t>(wgpu::DawnVkSemaphoreType::SyncFD) == 1u);
static_assert(static_cast<uint32_t>(wgpu::DawnVkSemaphoreType::ZirconHandle) == 2u);

#if DAWN_PLATFORM_IS(ANDROID)
// AHardwareBuffer
using ExternalMemoryHandle = struct AHardwareBuffer*;
#elif DAWN_PLATFORM_IS(LINUX)
// File descriptor
using ExternalMemoryHandle = int;
#elif DAWN_PLATFORM_IS(FUCHSIA)
// Really a Zircon vmo handle.
using ExternalMemoryHandle = zx_handle_t;
#else
// Generic types so that the Null service can compile, not used for real handles
using ExternalMemoryHandle = void*;
#endif

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_EXTERNALHANDLE_H_
