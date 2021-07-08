// Copyright 2021 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D11ON12UTIL_H_
#define DAWNNATIVE_D3D11ON12UTIL_H_

#include "dawn_native/d3d12/d3d12_platform.h"

#include <dawn_native/DawnNative.h>
#include <memory>
#include <unordered_set>

struct ID3D11On12Device;
struct IDXGIKeyedMutex;

namespace dawn_native { namespace d3d12 {

    void Flush11On12DeviceToAvoidLeaks(ComPtr<ID3D11On12Device> d3d11on12Device);

    // Wraps 11 wrapped resources in a cache.
    class D3D11on12ResourceCacheEntry {
      public:
        D3D11on12ResourceCacheEntry(ComPtr<ID3D11On12Device> d3d11on12Device);
        D3D11on12ResourceCacheEntry(ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex,
                                    ComPtr<ID3D11On12Device> d3d11on12Device);
        ~D3D11on12ResourceCacheEntry();

        ComPtr<IDXGIKeyedMutex> GetDXGIKeyedMutex();

        // Functors necessary for the
        // unordered_set<std::unique_ptr<D3D11on12ResourceCacheEntry>>-based cache.
        struct HashFunc {
            size_t operator()(const std::unique_ptr<D3D11on12ResourceCacheEntry>& a) const;
        };

        struct EqualityFunc {
            bool operator()(const std::unique_ptr<D3D11on12ResourceCacheEntry>& a,
                            const std::unique_ptr<D3D11on12ResourceCacheEntry>& b) const;
        };

      private:
        ComPtr<IDXGIKeyedMutex> mDXGIKeyedMutex;
        ComPtr<ID3D11On12Device> mD3D11on12Device;
    };

    // |D3D11on12ResourceCache| maintains a cache of 11 wrapped resources.
    // Each entry represents a 11 resource that is exclusively accessed by Dawn device.
    // Since each Dawn device creates and stores a 11on12 device, the 11on12 device
    // is used as the key for the cache entry which ensures only the same 11 wrapped
    // resource is re-used and also fully released.
    //
    // The cache is primarily needed to avoid repeatedly calling CreateWrappedResource
    // and special release code per ProduceTexture(device).
    class D3D11on12ResourceCache {
      public:
        D3D11on12ResourceCache();
        ~D3D11on12ResourceCache();

        ComPtr<IDXGIKeyedMutex> GetOrCreateDXGIKeyedMutex(WGPUDevice device,
                                                          ID3D12Resource* d3d12Resource);

      private:
        // TODO(dawn:625): Figure out a large enough cache size.
        static constexpr uint64_t kMaxD3D11on12ResourceCacheSize = 5;
        using Cache = std::unordered_set<std::unique_ptr<D3D11on12ResourceCacheEntry>,
                                         D3D11on12ResourceCacheEntry::HashFunc,
                                         D3D11on12ResourceCacheEntry::EqualityFunc>;

        Cache mCache;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D11ON12UTIL_H_
