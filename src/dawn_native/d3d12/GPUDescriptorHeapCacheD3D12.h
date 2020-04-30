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

#ifndef DAWNNATIVE_D3D12_GPUDESCRIPTORHEAPCACHE_H_
#define DAWNNATIVE_D3D12_GPUDESCRIPTORHEAPCACHE_H_

#include "dawn_native/d3d12/GPUDescriptorHeapAllocationD3D12.h"

#include <unordered_map>

namespace dawn_native { namespace d3d12 {

    using BindingInfoKey = size_t;

    // Wraps a GPU descriptor heap allocation using a key.
    struct GPUDescriptorHeapCacheEntry {
        GPUDescriptorHeapAllocation allocation;
        size_t refcount = 0;
        BindingInfoKey hash = 0;
    };

    // Caches GPUDescriptorHeapAllocation so that we don't create duplicate ones for every
    // BindGroup.
    class GPUDescriptorHeapCache {
      public:
        GPUDescriptorHeapCache() = default;

        GPUDescriptorHeapCacheEntry* Acquire(BindingInfoKey hash);
        void Release(GPUDescriptorHeapCacheEntry* entry);

      private:
        using Cache = std::unordered_map<BindingInfoKey, GPUDescriptorHeapCacheEntry>;

        Cache mCache;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_GPUDESCRIPTORHEAPCACHE_H_