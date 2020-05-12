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

#include "dawn_native/d3d12/SamplerHeapCacheD3D12.h"

#include "common/Assert.h"
#include "common/HashUtils.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/Forward.h"
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/StagingDescriptorAllocatorD3D12.h"

namespace dawn_native { namespace d3d12 {

    SamplerHeapCacheEntry::SamplerHeapCacheEntry(std::vector<Sampler*> samplers)
        : mSamplers(std::move(samplers)) {
    }

    SamplerHeapCacheEntry::SamplerHeapCacheEntry(SamplerHeapCache* cache,
                                                 StagingDescriptorAllocator* allocator,
                                                 std::vector<Sampler*> samplers,
                                                 CPUDescriptorHeapAllocation allocation)
        : mCPUAllocation(std::move(allocation)),
          mSamplers(std::move(samplers)),
          mAllocator(allocator),
          mCache(cache) {
        ASSERT(mCache != nullptr);
        ASSERT(mCPUAllocation.IsValid());
        ASSERT(!mSamplers.empty());
    }

    SamplerHeapCacheEntry::~SamplerHeapCacheEntry() {
        // If this is a blueprint then the cache cannot exist and has no entry to remove.
        if (mCache != nullptr) {
            mCache->RemoveCacheEntry(this);
        }
        ASSERT(!mCPUAllocation.IsValid());
    }

    void SamplerHeapCacheEntry::DeleteThis() {
        mAllocator->Deallocate(&mCPUAllocation);
        ASSERT(!mCPUAllocation.IsValid());
        RefCounted::DeleteThis();
    }

    bool SamplerHeapCacheEntry::Populate(ShaderVisibleDescriptorAllocator* allocator,
                                         BindGroup* group) {
        const BindGroupLayout* bgl = ToBackend(group->GetLayout());
        return group->Populate(allocator, bgl->GetSamplerDescriptorCount(),
                               D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, mCPUAllocation, &mGPUAllocation);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE SamplerHeapCacheEntry::GetBaseDescriptor() const {
        return mGPUAllocation.GetBaseDescriptor();
    }

    ResultOrError<SamplerHeapCacheEntry*> SamplerHeapCache::GetOrCreate(
        std::vector<Sampler*> samplers,
        StagingDescriptorAllocator* samplerAllocator) {
        SamplerHeapCacheEntry blueprint(samplers);
        auto iter = mCache.find(&blueprint);
        if (iter != mCache.end()) {
            (*iter)->Reference();
            return *iter;
        }

        CPUDescriptorHeapAllocation allocation;
        DAWN_TRY_ASSIGN(allocation, samplerAllocator->AllocateCPUDescriptors());

        const uint32_t samplerSizeIncrement = samplerAllocator->GetSizeIncrement();
        ID3D12Device* d3d12Device = mDevice->GetD3D12Device();

        for (uint32_t i = 0; i < samplers.size(); ++i) {
            const auto& samplerDesc = samplers[i]->GetSamplerDescriptor();
            d3d12Device->CreateSampler(&samplerDesc,
                                       allocation.OffsetFrom(samplerSizeIncrement, i));
        }

        SamplerHeapCacheEntry* entry = new SamplerHeapCacheEntry(
            this, samplerAllocator, std::move(samplers), std::move(allocation));
        mCache.insert(entry);
        return entry;
    }

    SamplerHeapCache::SamplerHeapCache(Device* device) : mDevice(device) {
    }

    SamplerHeapCache::~SamplerHeapCache() {
        ASSERT(mCache.empty());
    }

    void SamplerHeapCache::RemoveCacheEntry(SamplerHeapCacheEntry* entry) {
        ASSERT(entry->GetRefCountForTesting() == 0);
        size_t removedCount = mCache.erase(entry);
        ASSERT(removedCount == 1);
    }

    size_t SamplerHeapCacheEntry::HashFunc::operator()(const SamplerHeapCacheEntry* entry) const {
        size_t hash = 0;
        for (const Sampler* sampler : entry->mSamplers) {
            HashCombine(&hash, sampler);
        }
        return hash;
    }

    bool SamplerHeapCacheEntry::EqualityFunc::operator()(const SamplerHeapCacheEntry* a,
                                                         const SamplerHeapCacheEntry* b) const {
        return a->mSamplers == b->mSamplers;
    }
}}  // namespace dawn_native::d3d12