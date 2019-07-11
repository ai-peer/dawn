// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_RESOURCEHEAPD3D12_H_
#define DAWNNATIVE_D3D12_RESOURCEHEAPD3D12_H_

#include "dawn_native/ResourceHeap.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    // Wrapper for physical memory used with or without a resource object.
    class ResourceHeap : public ResourceHeapBase {
      public:
        ResourceHeap(ComPtr<ID3D12Heap> heap, D3D12_HEAP_TYPE heapType);

        ResourceHeap(ComPtr<ID3D12Resource> resource,
                     ComPtr<ID3D12Heap> heap,
                     D3D12_HEAP_TYPE heapType);

        ~ResourceHeap() = default;

        ComPtr<ID3D12Resource> GetD3D12Resource() const;
        ComPtr<ID3D12Heap> GetD3D12Heap() const;
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUPointer() const;

        D3D12_HEAP_TYPE GetD3D12HeapType() const;

      private:
        MaybeError MapImpl() override;
        void UnmapImpl() override;

        ComPtr<ID3D12Resource> mResource;
        ComPtr<ID3D12Heap> mHeap;

        D3D12_HEAP_TYPE mHeapType;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEHEAPD3D12_H_