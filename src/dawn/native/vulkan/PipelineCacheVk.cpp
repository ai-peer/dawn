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

#include "dawn/native/vulkan/PipelineCacheVk.h"

#include "dawn/native/Adapter.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/VulkanError.h"

namespace dawn::native::vulkan {

    // static
    Ref<PipelineCache> PipelineCache::Create(DeviceBase* device, const PipelineBase* pipeline) {
        Ref<PipelineCache> cache = AcquireRef(new PipelineCache(device, pipeline));
        // DAWN_TRY_WITH_CLEANUP(cache->Initialize(), {}, nullptr);
        return cache;
    }

    PipelineCache::PipelineCache(DeviceBase* device, const PipelineBase* pipeline)
        : ObjectBase(device),
          PipelineCacheBase(device->GetBlobCache(),
                            pipeline != nullptr ? pipeline->GetCacheKey() : device->GetCacheKey()) {
    }

    VkPipelineCache PipelineCache::GetHandle() const {
        return mHandle;
    }

    CachedBlob PipelineCache::SerializeToBlobImpl() {
        CachedBlob emptyBlob;
        if (mHandle == VK_NULL_HANDLE) {
            return emptyBlob;
        }

        size_t bufferSize;
        Device* device = ToBackend(GetDevice());
        DAWN_TRY_WITH_CLEANUP(
            CheckVkSuccess(device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle,
                                                           &bufferSize, nullptr),
                           "GetPipelineCacheData"),
            {}, emptyBlob);

        CachedBlob blob(bufferSize);
        DAWN_TRY_WITH_CLEANUP(
            CheckVkSuccess(device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle,
                                                           &bufferSize, blob.Get()),
                           "GetPipelineCacheData"),
            {}, emptyBlob);
        return blob;
    }

    MaybeError PipelineCache::Initialize() {
        const CachedBlob* blob = GetBlob();

        VkPipelineCacheCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.initialDataSize = blob->Size();
        createInfo.pInitialData = blob->Get();

        Device* device = ToBackend(GetDevice());
        DAWN_TRY(CheckVkSuccess(
            device->fn.CreatePipelineCache(device->GetVkDevice(), &createInfo, nullptr, &*mHandle),
            "CreatePipelineCache"));
        return {};
    }

    void PipelineCache::DeleteThis() {
        Device* device = ToBackend(GetDevice());
        if (mHandle != VK_NULL_HANDLE) {
            device->GetFencedDeleter()->DeleteWhenUnused(mHandle);
        }
        mHandle = VK_NULL_HANDLE;
        RefCounted::DeleteThis();
    }

}  // namespace dawn::native::vulkan
