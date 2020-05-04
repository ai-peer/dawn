// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_HEAPD3D12_H_
#define DAWNNATIVE_D3D12_HEAPD3D12_H_

#include "dawn_native/ResourceHeap.h"
#include "dawn_native/d3d12/PageableD3D12.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // This class is used to represent heap allocations, but also serves as a node within the
    // ResidencyManager's LRU cache. This node is inserted into the LRU-cache when it is first
    // allocated, and any time it is scheduled to be used by the GPU. This node is removed from the
    // LRU cache when it is evicted from resident memory due to budget constraints, or when the heap
    // is destroyed.
    class Heap : public ResourceHeapBase, public Pageable {
      public:
        Heap(ComPtr<ID3D12Pageable> d3d12Pageable, MemorySegment memorySegment, uint64_t size);
        ComPtr<ID3D12Heap> GetD3D12Heap() const;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_HEAPD3D12_H_