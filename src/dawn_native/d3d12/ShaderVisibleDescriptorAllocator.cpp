// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocator.h"

#include "common/Assert.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    // Implementation of ShaderVisibleDescriptorAllocation

    bool ShaderVisibleDescriptorAllocation::IsValid() const {
        return sizeIncrement != 0;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ShaderVisibleDescriptorAllocation::GetGPUHandle(
        uint32_t index) const {
        ASSERT(IsValid());

        D3D12_GPU_DESCRIPTOR_HANDLE handle = baseGPUHandle;
        handle.ptr += sizeIncrement * index;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ShaderVisibleDescriptorAllocation::GetCPUHandle(
        uint32_t index) const {
        ASSERT(IsValid());

        D3D12_CPU_DESCRIPTOR_HANDLE handle = baseCPUHandle;
        handle.ptr += sizeIncrement * index;
        return handle;
    }

    // Implementation of ShaderVisibleDescriptorAllocator

    ShaderVisibleDescriptorAllocator::ShaderVisibleDescriptorAllocator(Device* device)
        : mDevice(device) {
    }

    MaybeError ShaderVisibleDescriptorAllocator::Initialize() {
        return EnsureSpaceForFullPipelineLayout();
    }

    void ShaderVisibleDescriptorAllocator::Tick(uint64_t lastCompletedSerial) {
        // Do nothing because rolling around in the ring buffer would be an issue too.
    }

    MaybeError ShaderVisibleDescriptorAllocator::EnsureSpaceForFullPipelineLayout() {
        DAWN_TRY(RecreateHeap(&mCbvUavSrvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                              D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1));
        DAWN_TRY(RecreateHeap(&mSamplerHeap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                              D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE));
        return {};
    }

    ShaderVisibleDescriptorAllocation
    ShaderVisibleDescriptorAllocator::AllocateCbvUavSrcDescriptors(uint32_t count) {
        return AllocateDecriptor(&mCbvUavSrvHeap, count);
    }

    ShaderVisibleDescriptorAllocation ShaderVisibleDescriptorAllocator::AllocateSamplerDescriptors(
        uint32_t count) {
        return AllocateDecriptor(&mSamplerHeap, count);
    }

    ShaderVisibleDescriptorAllocator::Heaps ShaderVisibleDescriptorAllocator::GetCurrentHeaps()
        const {
        return {mCbvUavSrvHeap.heap.Get(), mSamplerHeap.heap.Get()};
    }

    ShaderVisibleDescriptorAllocator::HeapSerials
    ShaderVisibleDescriptorAllocator::GetCurrentHeapSerials() const {
        return {mCbvUavSrvHeap.serial, mSamplerHeap.serial};
    }

    ShaderVisibleDescriptorAllocation ShaderVisibleDescriptorAllocator::AllocateDecriptor(
        DescriptorHeapInfo* info,
        uint32_t count) {
        // When there are 0 descriptors we can return any valid allocation because the rest of the
        // code should never write into the descriptors.
        if (count == 0) {
            return info->wholeHeap;
        }

        uint64_t startOffset = info->ringBuffer.Allocate(count, 0);
        if (startOffset == RingBufferAllocator::kInvalidOffset) {
            // Return an invalid allocation.
            return {{0}, {0}, 0};
        }

        ShaderVisibleDescriptorAllocation result = info->wholeHeap;
        result.baseCPUHandle = result.GetCPUHandle(startOffset);
        result.baseGPUHandle = result.GetGPUHandle(startOffset);
        return result;
    }

    MaybeError ShaderVisibleDescriptorAllocator::RecreateHeap(DescriptorHeapInfo* info,
                                                              D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                              uint32_t allocationSize) {
        if (info->heap != nullptr) {
            mDevice->ReferenceUntilUnused(info->heap);
        }

        ID3D12Device* d3d12Device = mDevice->GetD3D12Device().Get();

        // Create the D3D12 heap
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = type;
        heapDescriptor.NumDescriptors = allocationSize;
        heapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDescriptor.NodeMask = 0;
        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateDescriptorHeap(&heapDescriptor, IID_PPV_ARGS(&info->heap)),
            "ID3D12Device::CreateDescriptorHeap"));

        // Update the rest of the info.
        info->wholeHeap = {info->heap->GetCPUDescriptorHandleForHeapStart(),
                           info->heap->GetGPUDescriptorHandleForHeapStart(),
                           d3d12Device->GetDescriptorHandleIncrementSize(type)};
        info->ringBuffer = RingBufferAllocator(allocationSize);
        info->serial++;

        return {};
    }

}}  // namespace dawn_native::d3d12
