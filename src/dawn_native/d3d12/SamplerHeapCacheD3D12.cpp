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
#include "dawn_native/d3d12/Forward.h"
#include "dawn_native/d3d12/StagingDescriptorAllocatorD3D12.h"

namespace dawn_native { namespace d3d12 {

    SamplerHeapCacheEntry::SamplerHeapCacheEntry(const BindGroupDescriptor* descriptor) {
        // BindGroups containing the same entries but in different order will not
        // use the same allocation in the cache.
        // TODO(dawn:155): Consider further optimization.
        for (uint32_t i = 0; i < descriptor->entryCount; ++i) {
            const BindGroupEntry& entry = descriptor->entries[i];
            if (entry.sampler != nullptr) {
                mSamplerInfo.push_back({entry.binding, ToBackend(entry.sampler)});
            }
        }
    }

    SamplerHeapCacheEntry::~SamplerHeapCacheEntry() {
        ASSERT(GetRefCountForTesting() == 1);
    }

    void SamplerHeapCacheEntry::ReleaseEntry(SamplerHeapCache* cache,
                                             StagingDescriptorAllocator* allocator) {
        ASSERT(cache != nullptr);
        ASSERT(GetRefCountForTesting() >= 1);
        if (Release() == 0) {
            if (IsValid()) {
                allocator->Deallocate(&mCPUAllocation);
            }
            ASSERT(!IsValid());
            cache->DestroyCacheEntry(this);
        }
    }

    bool SamplerHeapCacheEntry::IsValid() const {
        return mCPUAllocation.IsValid();
    }

    CPUDescriptorHeapAllocation* SamplerHeapCacheEntry::GetCPUAllocation() {
        return &mCPUAllocation;
    }

    GPUDescriptorHeapAllocation* SamplerHeapCacheEntry::GetGPUAllocation() {
        return &mGPUAllocation;
    }

    SamplerHeapCacheEntry* SamplerHeapCache::Acquire(const BindGroupDescriptor* descriptor) {
        SamplerHeapCacheEntry blueprint(descriptor);
        auto iter = mCache.find(&blueprint);
        if (iter != mCache.end()) {
            (*iter)->Reference();
            return *iter;
        }

        SamplerHeapCacheEntry* entry = new SamplerHeapCacheEntry(descriptor);
        mCache.insert(entry);
        return entry;
    }

    SamplerHeapCache::~SamplerHeapCache() {
        ASSERT(mCache.empty());
    }

    void SamplerHeapCache::DestroyCacheEntry(SamplerHeapCacheEntry* entry) {
        mCache.erase(entry);
    }

    size_t SamplerHeapCacheEntry::HashFunc::operator()(const SamplerHeapCacheEntry* entry) const {
        size_t hash = 0;
        for (const SamplerBindingInfo& samplerInfo : entry->mSamplerInfo) {
            HashCombine(&hash, samplerInfo.binding, samplerInfo.sampler);
        }
        return hash;
    }

    bool SamplerHeapCacheEntry::EqualityFunc::operator()(const SamplerHeapCacheEntry* a,
                                                         const SamplerHeapCacheEntry* b) const {
        if (a->mSamplerInfo.size() != b->mSamplerInfo.size()) {
            return false;
        }

        for (uint32_t i = 0; i < a->mSamplerInfo.size(); i++) {
            if (a->mSamplerInfo[i].binding != b->mSamplerInfo[i].binding ||
                a->mSamplerInfo[i].sampler != b->mSamplerInfo[i].sampler) {
                return false;
            }
        }

        return true;
    }
}}  // namespace dawn_native::d3d12