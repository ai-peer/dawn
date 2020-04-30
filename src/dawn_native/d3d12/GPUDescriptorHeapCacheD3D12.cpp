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
    GPUDescriptorHeapCacheEntry* GPUDescriptorHeapCache::Acquire(BindingInfoKey hash) {
        const auto& iter = mCache.find(hash);
        if (iter == mCache.cend()) {
            mCache[hash] = GPUDescriptorHeapCacheEntry{};
        }
        GPUDescriptorHeapCacheEntry& entry = mCache[hash];
        entry.refcount++;
        return &entry;
    }

    void GPUDescriptorHeapCache::Release(GPUDescriptorHeapCacheEntry* entry) {
        ASSERT(entry != nullptr);
        ASSERT(entry->refcount > 0);
        entry->refcount--;
        if (entry->refcount == 0) {
            mCache.erase(entry->hash);
        }

        // Invalidate the entry to ensure the caller doesn't use-after-free.
        entry = nullptr;
    }
}}  // namespace dawn_native::d3d12