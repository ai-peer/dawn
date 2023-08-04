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

#ifndef SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREFENCEVk_H_
#define SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREFENCEVk_H_

#include <vector>

#include "dawn/common/Platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/SharedFence.h"

namespace dawn::native::vulkan {

class Device;

class SharedFence final : public SharedFenceBase {
  public:
    static ResultOrError<Ref<SharedFence>> Create(
        Device* device,
        const char* label,
        const SharedFenceVkSemaphoreOpaqueFDDescriptor* descriptor);

    static ResultOrError<Ref<SharedFence>> Create(
        Device* device,
        const char* label,
        const SharedFenceVkSemaphoreSyncFDDescriptor* descriptor);

    static ResultOrError<Ref<SharedFence>> Create(
        Device* device,
        const char* label,
        const SharedFenceVkSemaphoreZirconHandleDescriptor* descriptor);

#if DAWN_PLATFORM_IS(FUCHSIA)
    using Handle = uint32_t;
#else
    using Handle = int;
#endif

  private:
    SharedFence(Device* device, const char* label, Handle handle);
    void DestroyImpl() override;

    MaybeError ExportInfoImpl(SharedFenceExportInfo* info) const override;

    Handle mHandle;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREFENCEVk_H_
