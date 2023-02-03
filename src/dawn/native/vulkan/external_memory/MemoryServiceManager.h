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

#ifndef SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICEMANAGER_H_
#define SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICEMANAGER_H_

#include <memory>
#include <unordered_map>

#include "dawn/native/vulkan/external_memory/MemoryService.h"

namespace dawn::native::vulkan {
class Device;
struct VulkanDeviceInfo;
}  // namespace dawn::native::vulkan

namespace dawn::native::vulkan::external_memory {

class ServiceManager {
  public:
    explicit ServiceManager(Device* device);
    ~ServiceManager() = default;

    static bool CheckSupport(const VulkanDeviceInfo& deviceInfo);

    Service* GetService(ExternalImageType type);

  private:
    Device* mDevice = nullptr;
    std::unordered_map<ExternalImageType, std::unique_ptr<Service>> services_;
    std::unique_ptr<Service> null_service_;
};

}  // namespace dawn::native::vulkan::external_memory

#endif  // SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICEMANAGER_H_
