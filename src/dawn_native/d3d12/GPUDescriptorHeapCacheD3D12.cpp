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

#include "dawn_native/d3d12/GPUDescriptorHeapCacheD3D12.h"

#include "common/Assert.h"

namespace dawn_native { namespace d3d12 {

    GPUDescriptorHeapCacheEntry::GPUDescriptorHeapCacheEntry(BindingInfoKey hash) : mHash(hash) {
    }

    GPUDescriptorHeapCacheEntry::~GPUDescriptorHeapCacheEntry() {
        ASSERT(GetRefCountForTesting() == 1);
    }

    void GPUDescriptorHeapCacheEntry::ReleaseEntry(GPUDescriptorHeapCache* cache) {
        ASSERT(cache != nullptr);
        ASSERT(GetRefCountForTesting() > 1);
        if (Release() == 1) {
            cache->DestoryCacheEntry(this);
        }
    }

    GPUDescriptorHeapAllocation* GPUDescriptorHeapCacheEntry::GetAllocation() {
        return &mAllocation;
    }

    GPUDescriptorHeapCacheEntry* GPUDescriptorHeapCache::Acquire(BindingInfoKey hash) {
        if (mCache.find(hash) == mCache.end()) {
            mCache.emplace(hash, hash);
        }
        GPUDescriptorHeapCacheEntry& entry = mCache[hash];
        entry.Reference();
        return &entry;
    }

    GPUDescriptorHeapCache::~GPUDescriptorHeapCache() {
        ASSERT(mCache.empty());
    }

    void GPUDescriptorHeapCache::DestoryCacheEntry(GPUDescriptorHeapCacheEntry* entry) {
        mCache.erase(entry->mHash);
    }
}}  // namespace dawn_native::d3d12