// Copyright 2020 The Dawn Authors
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

#include "dawn_native/vulkan/PipelineCacheVk.h"

#include "common/HashUtils.h"

#include "dawn_native/vulkan/AdapterVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"

#include <sstream>

namespace dawn_native { namespace vulkan {

    PipelineCache::PipelineCache(Device* device) : PipelineCacheBase(device) {
        // Generate a string used for the key with this pipeline cache.
        std::stringstream stream;
        stream << GetMetadataForKey();

        // Append the pipeline cache ID to ensure the retrieved pipeline cache is compatible.
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html
        const PCIExtendedInfo& info = ToBackend(device->GetAdapter())->GetPCIExtendedInfo();
        for (const uint32_t x : info.pipelineCacheUUID) {
            stream << std::hex << x;
        }

        // TODO: Replace this with a secure hash?
        size_t hash = Hash(stream.str());
        mPipelineCacheKey = PersistentCache::CreateKey(hash);
    }

    PipelineCache::~PipelineCache() {
        if (mIsPipelineCacheLoaded) {
            ASSERT(mHandle != VK_NULL_HANDLE);
            Device* device = ToBackend(mDevice);
            device->fn.DestroyPipelineCache(device->GetVkDevice(), mHandle, nullptr);
            mHandle = VK_NULL_HANDLE;
        }
    }

    MaybeError PipelineCache::loadPipelineCacheIfNecessary() {
        if (mIsPipelineCacheLoaded) {
            return {};
        }

        std::unique_ptr<uint8_t[]> data;
        const size_t cacheSize = mDevice->GetPersistentCache()->getDataSize(mPipelineCacheKey);
        if (cacheSize > 0) {
            data.reset(new uint8_t[cacheSize]);
            mDevice->GetPersistentCache()->loadData(mPipelineCacheKey, data.get(), cacheSize);
        }

        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.initialDataSize = cacheSize;
        createInfo.pInitialData = data.get();

        ASSERT(data == nullptr || cacheSize > 0);

        // TODO: Consider giving this an allocator if memory usage is high.
        Device* device = ToBackend(mDevice);
        DAWN_TRY(CheckVkSuccess(
            device->fn.CreatePipelineCache(device->GetVkDevice(), &createInfo, nullptr, &*mHandle),
            "vkCreatePipelineCache"));

        mIsPipelineCacheLoaded = true;

        return {};
    }

    MaybeError PipelineCache::storePipelineCache() {
        if (!mIsPipelineCacheLoaded) {
            return {};
        }

        Device* device = ToBackend(mDevice);

        // vkGetPipelineCacheData has two operations. One to get the cache size (where pData is
        // nullptr) and the other to get the cache data (pData != null and size > 0).
        size_t cacheSize = 0;
        DAWN_TRY(CheckVkSuccess(
            device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle, &cacheSize, nullptr),
            "vkCreatePipelineCache"));

        ASSERT(cacheSize > 0);

        // vkGetPipelineCacheData can partially write cache data. Since the written data size will
        // be saved in |cacheSize|, load the pipeline cache into a zeroed buffer that is the maximum
        // size then store only the written data.
        std::vector<uint8_t> writtenData(cacheSize, 0);
        VkResult result = VkResult::WrapUnsafe(device->fn.GetPipelineCacheData(
            device->GetVkDevice(), mHandle, &cacheSize, writtenData.data()));
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            return DAWN_INTERNAL_ERROR("vkGetPipelineCacheData");
        }

        // Written cache data cannot exceed the cache size.
        ASSERT(cacheSize <= writtenData.size());

        // Written data should be at-least the size of the cache version header.
        // See VK_PIPELINE_CACHE_HEADER_VERSION_ONE in vkGetPipelineCacheData.
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPipelineCacheData.html
        ASSERT(cacheSize > 16 + VK_UUID_SIZE);

        mDevice->GetPersistentCache()->storeData(mPipelineCacheKey, writtenData.data(), cacheSize);

        return {};
    }

    ResultOrError<VkPipelineCache> PipelineCache::GetVkPipelineCache() {
        DAWN_TRY(loadPipelineCacheIfNecessary());
        return std::move(mHandle);
    }

}}  // namespace dawn_native::vulkan