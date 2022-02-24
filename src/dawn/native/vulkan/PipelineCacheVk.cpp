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
    Ref<PipelineCache> PipelineCache::Create(PipelineBase* pipeline) {
        // TODO(dawn:549) Add isolation key to the create key once plumbed through the device.
        PersistentCache::Key key = PersistentCache::CreateKey(PersistentCache::KeyType::Pipeline,
                                                              "", pipeline->GetContentHash());
        Ref<PipelineCache> cache = AcquireRef(new PipelineCache(
            pipeline->GetDevice(), pipeline->GetDevice()->GetAdapter()->GetPersistentCache(),
            std::move(key)));
        if (cache->Initialize()) {
            return cache;
        }
        return nullptr;
    }

    VkPipelineCache PipelineCache::GetHandle() const {
        return mHandle;
    }

    ScopedCachedBlob PipelineCache::SerializeToBlobImpl() {
        if (mHandle == VK_NULL_HANDLE) {
            return {};
        }

        Device* device = ToBackend(GetDevice());
        ScopedCachedBlob blob = {};
        DAWN_TRY_WITH_CLEANUP(
            CheckVkSuccess(device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle,
                                                           &blob.bufferSize, nullptr),
                           "GetPipelineCacheData"),
            {}, ScopedCachedBlob{});
        blob.buffer.reset(new uint8_t[blob.bufferSize]);
        DAWN_TRY_WITH_CLEANUP(
            CheckVkSuccess(device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle,
                                                           &blob.bufferSize, blob.buffer.get()),
                           "GetPipelineCacheData"),
            {}, ScopedCachedBlob{});
        return blob;
    }

    bool PipelineCache::Initialize() {
        if (!PipelineCacheBase::Initialize()) {
            return false;
        }
        const ScopedCachedBlob* blob = GetBlob();

        VkPipelineCacheCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.initialDataSize = blob->bufferSize;
        createInfo.pInitialData = blob->buffer.get();

        Device* device = ToBackend(GetDevice());
        DAWN_TRY_WITH_CLEANUP(
            CheckVkSuccess(device->fn.CreatePipelineCache(device->GetVkDevice(), &createInfo,
                                                          nullptr, &*mHandle),
                           "CreatePipelineCache"),
            {}, false);
        return true;
    }

}  // namespace dawn::native::vulkan
