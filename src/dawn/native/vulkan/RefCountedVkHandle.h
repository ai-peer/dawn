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

#ifndef SRC_DAWN_NATIVE_VULKAN_REFCOUNTEDVKHANDLE_H_
#define SRC_DAWN_NATIVE_VULKAN_REFCOUNTEDVKHANDLE_H_

#include "dawn/common/RefCounted.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"

namespace dawn::native::vulkan {

template <typename Handle>
class RefCountedVkHandle : public RefCounted {
  public:
    RefCountedVkHandle(Device* device, Handle handle) : mDevice(device), mHandle(handle) {}

    ~RefCountedVkHandle() override {
        if (mHandle != VK_NULL_HANDLE) {
            mDevice->GetFencedDeleter()->DeleteWhenUnused(mHandle);
        }
    }

    Handle Get() const { return mHandle; }

  private:
    Ref<Device> mDevice;
    Handle mHandle = VK_NULL_HANDLE;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_REFCOUNTEDVKHANDLE_H_
