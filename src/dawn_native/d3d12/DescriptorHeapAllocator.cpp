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

#include "dawn_native/d3d12/DescriptorHeapAllocator.h"

#include "common/Assert.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    DescriptorHeapHandle::DescriptorHeapHandle()
        : mDescriptorHeap(nullptr), mSizeIncrement(0), mOffset(0) {
    }

    DescriptorHeapHandle::DescriptorHeapHandle(ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                               uint32_t sizeIncrement,
                                               uint32_t offset)
        : mDescriptorHeap(descriptorHeap), mSizeIncrement(sizeIncrement), mOffset(offset) {
    }

    ID3D12DescriptorHeap* DescriptorHeapHandle::Get() const {
        return mDescriptorHeap.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapHandle::GetCPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapHandle::GetGPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    DescriptorHeap::DescriptorHeap(uint32_t size,
                                   D3D12_DESCRIPTOR_HEAP_TYPE type,
                                   D3D12_DESCRIPTOR_HEAP_FLAGS flags,
                                   Device* device)
        : StagingBufferBase(size), mFlags(flags), mType(type), mDevice(device) {
    }

    ID3D12DescriptorHeap* DescriptorHeap::GetDescriptorHeap() const {
        return mDescriptorHeap.Get();
    }

    MaybeError DescriptorHeap::Initialize() {
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = mType;
        heapDescriptor.NumDescriptors = GetSize();
        heapDescriptor.Flags = mFlags;
        heapDescriptor.NodeMask = 0;
        if (FAILED(mDevice->GetD3D12Device()->CreateDescriptorHeap(
                &heapDescriptor, IID_PPV_ARGS(&mDescriptorHeap)))) {
            return DAWN_CONTEXT_LOST_ERROR("Unable to  allocate descriptor heap");
        }

        return {};
    }

    DescriptorHeapAllocator::DescriptorHeapAllocator(Device* device)
        : mDevice(device),
          mSizeIncrements{
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
          } {
    }

    DescriptorHeapHandle DescriptorHeapAllocator::Allocate(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                           uint32_t count,
                                                           uint32_t allocationSize,
                                                           std::unique_ptr<RingBuffer>& buffer,
                                                           D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
        if (count == 0) {
            return DescriptorHeapHandle();
        }

        size_t offset = (buffer == nullptr) ? INVALID_OFFSET : buffer->SubAllocate(count);
        if (offset != INVALID_OFFSET) {
            DescriptorHeapHandle handle(
                static_cast<DescriptorHeap*>(buffer->GetStagingBuffer())->GetDescriptorHeap(),
                mSizeIncrements[type], offset);
            return handle;
        }

        std::unique_ptr<StagingBufferBase> descriptorHeap =
            std::make_unique<DescriptorHeap>(allocationSize, type, flags, mDevice);
        DAWN_UNUSED(descriptorHeap->Initialize());  // TODO: MaybeError?

        buffer = std::make_unique<RingBuffer>(mDevice, descriptorHeap.release());
        offset = buffer->SubAllocate(count);
        ASSERT(offset != INVALID_OFFSET);

        DescriptorHeapHandle handle(
            static_cast<DescriptorHeap*>(buffer->GetStagingBuffer())->GetDescriptorHeap(),
            mSizeIncrements[type], offset);
        return handle;
    }

    DescriptorHeapHandle DescriptorHeapAllocator::AllocateCPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                                  uint32_t count) {
        return Allocate(type, count, count, mCpuDescriptorHeapInfos[type],
                        D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    }

    DescriptorHeapHandle DescriptorHeapAllocator::AllocateGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                                  uint32_t count) {
        ASSERT(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
               type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        unsigned int heapSize =
            (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? kMaxCbvUavSrvHeapSize
                                                            : kMaxSamplerHeapSize);
        return Allocate(type, count, heapSize, mGpuDescriptorHeapInfos[type],
                        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    }

    void DescriptorHeapAllocator::Tick(uint64_t lastCompletedSerial) {
        for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
            if (mCpuDescriptorHeapInfos[i] != nullptr) {
                mCpuDescriptorHeapInfos[i]->Tick(lastCompletedSerial);
            }

            if (mGpuDescriptorHeapInfos[i] != nullptr) {
                mGpuDescriptorHeapInfos[i]->Tick(lastCompletedSerial);
            }
        }

        mReleasedHandles.ClearUpTo(lastCompletedSerial);
    }

    void DescriptorHeapAllocator::Release(DescriptorHeapHandle handle) {
        mReleasedHandles.Enqueue(handle, mDevice->GetPendingCommandSerial());
    }
}}  // namespace dawn_native::d3d12
