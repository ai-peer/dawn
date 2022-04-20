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

#ifndef SRC_DAWN_NATIVE_VULKAN_PIPELINECACHEVK_H_
#define SRC_DAWN_NATIVE_VULKAN_PIPELINECACHEVK_H_

#include "dawn/native/ObjectBase.h"
#include "dawn/native/PipelineCache.h"

#include "dawn/common/vulkan_platform.h"

namespace dawn::native {
    class DeviceBase;
}

namespace dawn::native::vulkan {

    class PipelineCache final : public PipelineCacheBase {
      public:
        // Creation function takes both the device and the pipeline. By default, the pipeline can be
        // nullptr to support monolithic caches that do not need an explicit pipeline to start.
        static Ref<PipelineCache> Create(DeviceBase* device,
                                         const PipelineBase* pipeline = nullptr);

        DeviceBase* GetDevice() const;
        VkPipelineCache GetHandle() const;

        // Returns whether or not the cache was a cache hit from the blob cache.
        bool CacheHit() const;

      private:
        explicit PipelineCache(DeviceBase* device, const CacheKey& key);

        void Initialize();
        CachedBlob SerializeToBlobImpl() override;

        void DeleteThis() override;

        DeviceBase* mDevice;
        VkPipelineCache mHandle = VK_NULL_HANDLE;
        bool mCacheHit = false;
    };

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_PIPELINECACHEVK_H_
