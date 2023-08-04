// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/vulkan/SharedFenceVk.h"

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/vulkan/DeviceVk.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include <zircon/syscalls.h>
#elif DAWN_PLATFORM_IS(POSIX)
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
    DAWN_INVALID_IF(status != ZX_OK, "Failed to duplicate zircon fence handle (%u)",
                    descriptor->handle);
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
#if DAWN_PLATFORM_IS(POSIX)
    int fd = dup(descriptor->handle);
    DAWN_INVALID_IF(fd < 0, "Failed to dup fence sync fd (%d)", descriptor->handle);
    auto fence = AcquireRef(new SharedFence(device, label, fd));
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
#if DAWN_PLATFORM_IS(POSIX)
    int fd = dup(descriptor->handle);
    DAWN_INVALID_IF(fd < 0, "Failed to dup fence opaque fd (%d)", descriptor->handle);
    auto fence = AcquireRef(new SharedFence(device, label, fd));
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
#elif DAWN_PLATFORM_IS(POSIX)
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
#elif DAWN_PLATFORM_IS(POSIX)
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
