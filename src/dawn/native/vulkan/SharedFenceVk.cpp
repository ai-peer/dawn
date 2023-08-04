// Copyright 2023 The Dawn Authors
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

#include "dawn/native/vulkan/SharedFenceVk.h"

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/vulkan/DeviceVk.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include <zircon/syscalls.h>
#elif DAWN_PLATFORM_IS(LINUX)
#include <unistd.h>
#endif

namespace dawn::native::vulkan {

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceVkSemaphoreZirconHandleDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle == 0, "Zircon handle (%d) was invalid.", descriptor->handle);

#if DAWN_PLATFORM_IS(FUCHSIA)
    zx_handle_t out_handle = ZX_HANDLE_INVALID;
    zx_status_t status = zx_handle_duplicate(descriptor->handle, ZX_RIGHT_SAME_RIGHTS, &out_handle);
    DAWN_ASSERT(status == ZX_OK);
    auto fence = AcquireRef(new SharedFence(device, label, out_handle));
    fence->mType = wgpu::SharedFenceType::VkSemaphoreZirconHandle;
    return fence;
#else
    DAWN_UNREACHABLE();
#endif
}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceVkSemaphoreSyncFDDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle < 0, "File descriptor (%d) was invalid.",
                    descriptor->handle);
#if DAWN_PLATFORM_IS(LINUX)
    auto fence = AcquireRef(new SharedFence(device, label, dup(descriptor->handle)));
    fence->mType = wgpu::SharedFenceType::VkSemaphoreSyncFD;
    return fence;
#else
    DAWN_UNREACHABLE();
#endif
}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceVkSemaphoreOpaqueFDDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle < 0, "File descriptor (%d) was invalid.",
                    descriptor->handle);
#if DAWN_PLATFORM_IS(LINUX)
    auto fence = AcquireRef(new SharedFence(device, label, dup(descriptor->handle)));
    fence->mType = wgpu::SharedFenceType::VkSemaphoreOpaqueFD;
    return fence;
#else
    DAWN_UNREACHABLE();
#endif
}

SharedFence::SharedFence(Device* device, const char* label, Handle handle)
    : SharedFenceBase(device, label), mHandle(handle) {}

void SharedFence::DestroyImpl() {
#if DAWN_PLATFORM_IS(FUCHSIA)
    zx_status_t status = zx_handle_close(mHandle);
    DAWN_ASSERT(status == ZX_OK);
#elif DAWN_PLATFORM_IS(LINUX)
    int ret = close(mHandle);
    DAWN_ASSERT(ret != -1);
#endif
}

MaybeError SharedFence::ExportInfoImpl(SharedFenceExportInfo* info) const {
    info->type = mType;

#if DAWN_PLATFORM_IS(FUCHSIA)
    DAWN_TRY(ValidateSingleSType(info->nextInChain,
                                 wgpu::SType::SharedFenceVkSemaphoreZirconHandleExportInfo));

    SharedFenceVkSemaphoreZirconHandleExportInfo* exportInfo = nullptr;
    FindInChain(info->nextInChain, &exportInfo);

    if (exportInfo != nullptr) {
        exportInfo->handle = mHandle;
    }
#elif DAWN_PLATFORM_IS(LINUX)
    switch (mType) {
        case wgpu::SharedFenceType::VkSemaphoreSyncFD:
            DAWN_TRY(ValidateSingleSType(info->nextInChain,
                                         wgpu::SType::SharedFenceVkSemaphoreSyncFDExportInfo));
            {
                SharedFenceVkSemaphoreSyncFDExportInfo* exportInfo = nullptr;
                FindInChain(info->nextInChain, &exportInfo);

                if (exportInfo != nullptr) {
                    exportInfo->handle = mHandle;
                }
            }
            break;
        case wgpu::SharedFenceType::VkSemaphoreOpaqueFD:
            DAWN_TRY(ValidateSingleSType(info->nextInChain,
                                         wgpu::SType::SharedFenceVkSemaphoreOpaqueFDExportInfo));
            {
                SharedFenceVkSemaphoreOpaqueFDExportInfo* exportInfo = nullptr;
                FindInChain(info->nextInChain, &exportInfo);

                if (exportInfo != nullptr) {
                    exportInfo->handle = mHandle;
                }
            }
            break;
        default:
            DAWN_UNREACHABLE();
    }
#else
    DAWN_UNUSED(mHandle);
    DAWN_UNREACHABLE();
#endif
    return {};
}

}  // namespace dawn::native::vulkan
