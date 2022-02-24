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

#ifndef DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_
#define DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_

#include "dawn/native/PipelineCache.h"

#include "dawn/common/vulkan_platform.h"

namespace dawn::native::vulkan {

    class PipelineCache final : public PipelineCacheBase {
      public:
        static Ref<PipelineCache> Create(PipelineBase* pipeline);

        VkPipelineCache GetHandle() const;
        ResultOrError<ScopedCachedBlob> GetCacheData();

      private:
        using PipelineCacheBase::PipelineCacheBase;

        bool Initialize() override;
        ScopedCachedBlob SerializeToBlobImpl() override;

        VkPipelineCache mHandle = VK_NULL_HANDLE;
    };

}  // namespace dawn::native::vulkan

#endif  // DAWNNATIVE_VULKAN_PIPELINECACHEVK_H_
