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

#include "dawn_native/d3d12/ResourceAllocatorD3D12.h"

#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {

        static constexpr D3D12_HEAP_PROPERTIES kDefaultHeapProperties = {
            D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};

        static constexpr D3D12_HEAP_PROPERTIES kUploadHeapProperties = {
            D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};

        static constexpr D3D12_HEAP_PROPERTIES kReadbackHeapProperties = {
            D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};

        D3D12_RESOURCE_STATES GetHeapState(D3D12_HEAP_TYPE heapType) {
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

            // D3D12 requires buffers on the READBACK heap to have the
            // D3D12_RESOURCE_STATE_COPY_DEST state
            if (heapType == D3D12_HEAP_TYPE_READBACK) {
                initialState |= D3D12_RESOURCE_STATE_COPY_DEST;
            }

            // D3D12 requires buffers on the UPLOAD heap to have the
            // D3D12_RESOURCE_STATE_GENERIC_READ state
            if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
                initialState |= D3D12_RESOURCE_STATE_GENERIC_READ;
            }

            return initialState;
        }

        D3D12_HEAP_PROPERTIES GetHeapProperties(D3D12_HEAP_TYPE heapType) {
            switch (heapType) {
                case D3D12_HEAP_TYPE_UPLOAD:
                    return kUploadHeapProperties;
                case D3D12_HEAP_TYPE_READBACK:
                    return kReadbackHeapProperties;
                default:
                    return kDefaultHeapProperties;
            }
        }
    }  // namespace

    ResourceHeapAllocator::ResourceHeapAllocator(Device* device, D3D12_HEAP_TYPE heapType)
        : mDevice(device), mHeapType(heapType) {
    }

    ResourceHeapBase* ResourceHeapAllocator::CreateHeap(size_t heapSize, int heapFlags) {
        D3D12_HEAP_DESC heapDesc;
        heapDesc.SizeInBytes = heapSize;
        heapDesc.Properties = GetHeapProperties(mHeapType);
        heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        heapDesc.Flags = static_cast<D3D12_HEAP_FLAGS>(heapFlags);

        ComPtr<ID3D12Heap> heap;
        if (FAILED(mDevice->GetD3D12Device()->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)))) {
            return nullptr;
        }

        return new ResourceHeap(heap, mHeapType);
    }

    void ResourceHeapAllocator::FreeHeap(ResourceHeapBase* heap) {
        ASSERT(heap != nullptr);
        // Heaps may still be in use on the GPU. Enqueue them so that we hold onto them until
        // GPU execution has completed
        mReleasedHeaps.Enqueue(std::unique_ptr<ResourceHeapBase>(heap),
                               mDevice->GetPendingCommandSerial());
    }

    void ResourceHeapAllocator::Tick(uint64_t lastCompletedSerial) {
        mReleasedHeaps.ClearUpTo(lastCompletedSerial);
    }

    // PlacedResourceAllocator

    PlacedResourceAllocator::PlacedResourceAllocator(Device* device, D3D12_HEAP_TYPE heapType)
        : mDevice(device), mIsDirect(true), mDirectAllocator(device, heapType) {
    }

    PlacedResourceAllocator::PlacedResourceAllocator(size_t maxBlockSize,
                                                     size_t resourceHeapSize,
                                                     Device* device,
                                                     D3D12_HEAP_TYPE heapType)
        : mDevice(device),
          mIsDirect(false),
          mSubAllocator(maxBlockSize, resourceHeapSize, device, heapType) {
    }

    ResourceMemoryAllocation PlacedResourceAllocator::Allocate(
        D3D12_RESOURCE_DESC resourceDescriptor,
        size_t allocationSize,
        D3D12_HEAP_FLAGS heapFlags) {
        ASSERT(IsAligned(allocationSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

        // Direct allocation vs Sub-allocation
        //
        // Direct allocation using placed resources: place a single resource in a heap that is of
        // equal size. Each direct allocation owns a heap and resource whose lifetime is managed by
        // the client.
        //
        // Sub-allocation using placed resources: placed one or more resources in the same heap of
        // larger size. Each sub-allocation owns only the resource and holds a reference to the heap
        // whose lifetime is managed by the allocator.

        // Create the heap-backed allocation.
        const ResourceMemoryAllocation invalid = ResourceMemoryAllocation{INVALID_OFFSET};
        const ResourceMemoryAllocation heapAllocation =
            (mIsDirect) ? mDirectAllocator.Allocate(allocationSize, heapFlags)
                        : mSubAllocator.Allocate(allocationSize, heapFlags);
        if (heapAllocation.GetOffset() == INVALID_OFFSET) {
            return invalid;
        }

        const D3D12_HEAP_TYPE heapType =
            ToBackend(heapAllocation.GetResourceHeap())->GetD3D12HeapType();

        // Resources are placed relative to the heap offset but otherwise allocated to the absoluate
        // offset of the allocator.
        const size_t placedOffset =
            (mIsDirect) ? 0 : (heapAllocation.GetOffset() % mSubAllocator.GetResourceHeapSize());

        ID3D12Heap* heap = ToBackend(heapAllocation.GetResourceHeap())->GetD3D12Heap();

        ComPtr<ID3D12Resource> placedResource;
        if (FAILED(mDevice->GetD3D12Device()->CreatePlacedResource(
                heap, placedOffset, &resourceDescriptor, GetHeapState(heapType), nullptr,
                IID_PPV_ARGS(&placedResource)))) {
            return invalid;
        }

        if (mIsDirect) {
            ToBackend(heapAllocation.GetResourceHeap())->PlaceResource(placedResource);
            return heapAllocation;
        } else {
            // Create a new "placed" resource heap with a weak reference to the physical resource
            // heap.
            std::unique_ptr<ResourceHeap> placedResourceHeap =
                std::make_unique<ResourceHeap>(heap, heapType);
            placedResourceHeap->PlaceResource(placedResource);

            return ResourceMemoryAllocation{heapAllocation.GetOffset(),
                                            placedResourceHeap.release()};
        }
    }

    void PlacedResourceAllocator::Deallocate(ResourceMemoryAllocation allocation) {
        // Direct allocation owns both the resource and heap.
        if (mIsDirect) {
            mDirectAllocator.Deallocate(allocation);
        }
        // Sub-allocation owns the resource and not the heap.
        else {
            std::unique_ptr<ResourceHeapBase> placedResourceHeap(allocation.GetResourceHeap());
            mReleasedResources.Enqueue(std::move(placedResourceHeap),
                                       mDevice->GetPendingCommandSerial());

            mSubAllocator.Deallocate(allocation);
        }
    }

    void PlacedResourceAllocator::Tick(uint64_t lastCompletedSerial) {
        mReleasedResources.ClearUpTo(lastCompletedSerial);

        return (mIsDirect) ? mDirectAllocator.GetResourceHeapAllocator().Tick(lastCompletedSerial)
                           : mSubAllocator.GetResourceHeapAllocator().Tick(lastCompletedSerial);
    }
}}  // namespace dawn_native::d3d12
