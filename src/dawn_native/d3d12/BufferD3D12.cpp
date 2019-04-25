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

#include "dawn_native/d3d12/BufferD3D12.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/ResourceHeapD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {

        D3D12_RESOURCE_STATES D3D12BufferUsage(dawn::BufferUsageBit usage) {
            D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

            if (usage & dawn::BufferUsageBit::TransferSrc) {
                resourceState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
            }
            if (usage & dawn::BufferUsageBit::TransferDst) {
                resourceState |= D3D12_RESOURCE_STATE_COPY_DEST;
            }
            if (usage & (dawn::BufferUsageBit::Vertex | dawn::BufferUsageBit::Uniform)) {
                resourceState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }
            if (usage & dawn::BufferUsageBit::Index) {
                resourceState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
            }
            if (usage & dawn::BufferUsageBit::Storage) {
                resourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            return resourceState;
        }

        D3D12_HEAP_TYPE D3D12HeapType(dawn::BufferUsageBit allowedUsage) {
            if (allowedUsage & dawn::BufferUsageBit::MapRead) {
                return D3D12_HEAP_TYPE_READBACK;
            } else if (allowedUsage & dawn::BufferUsageBit::MapWrite) {
                return D3D12_HEAP_TYPE_UPLOAD;
            } else {
                return D3D12_HEAP_TYPE_DEFAULT;
            }
        }

        uint32_t D3D12MemoryTypeBits(D3D12_HEAP_TYPE heapType) {
            switch (heapType) {
                case D3D12_HEAP_TYPE_DEFAULT:
                    return 1;
                case D3D12_HEAP_TYPE_UPLOAD:
                    return 2;
                case D3D12_HEAP_TYPE_READBACK:
                    return 4;
                case D3D12_HEAP_TYPE_CUSTOM:  // TODO
                default:
                    UNREACHABLE();
            }
        }
    }  // namespace

    Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
        : BufferBase(device, descriptor) {
        if (GetDevice()->ConsumedError(Initialize())) {
            return;
        }
    }

    // TODO(bryan.bernhart@intel.com): Move into BufferBase.
    MaybeError Buffer::Initialize() {
        auto heapType = D3D12HeapType(GetUsage());

        // D3D12 requires buffers on the READBACK heap to have the D3D12_RESOURCE_STATE_COPY_DEST
        // state
        if (heapType == D3D12_HEAP_TYPE_READBACK) {
            mFixedResourceState = true;
            mLastUsage = dawn::BufferUsageBit::TransferDst;
            mAllocatorType = AllocatorType::Upload;
        }

        // D3D12 requires buffers on the UPLOAD heap to have the D3D12_RESOURCE_STATE_GENERIC_READ
        // state
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            mFixedResourceState = true;
            mLastUsage = dawn::BufferUsageBit::TransferSrc;
            mAllocatorType = AllocatorType::Upload;
        }

        // TODO(bryan.bernhart@intel.com): Move into DeviceBase when supported.
        DAWN_TRY_ASSIGN(mAllocation, ToBackend(GetDevice())
                                         ->GetSubAllocation(D3D12MemoryTypeBits(heapType),
                                                            mAllocatorType, GetD3D12Size()));
        return {};
    }

    Buffer::~Buffer() {
        DestroyInternal();
    }

    uint32_t Buffer::GetD3D12Size() const {
        // TODO(enga@google.com): TODO investigate if this needs to be a constraint at the API level
        return Align(GetSize(), 256);
    }

    ComPtr<ID3D12Resource> Buffer::GetD3D12Resource() {
        return ToBackend(mAllocation.GetResourceHeap())->GetD3D12Resource();
    }

    void Buffer::TransitionUsageNow(ComPtr<ID3D12GraphicsCommandList> commandList,
                                    dawn::BufferUsageBit usage) {
        // Resources in upload and readback heaps must be kept in the COPY_SOURCE/DEST state
        if (mFixedResourceState) {
            ASSERT(usage == mLastUsage);
            return;
        }

        // We can skip transitions to already current usages.
        // TODO(cwallez@chromium.org): Need some form of UAV barriers at some point.
        bool lastIncludesTarget = (mLastUsage & usage) == usage;
        if (lastIncludesTarget) {
            return;
        }

        D3D12_RESOURCE_STATES lastState = D3D12BufferUsage(mLastUsage);
        D3D12_RESOURCE_STATES newState = D3D12BufferUsage(usage);

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = GetD3D12Resource().Get();
        barrier.Transition.StateBefore = lastState;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = 0;

        commandList->ResourceBarrier(1, &barrier);

        mLastUsage = usage;
    }

    D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetVA() const {
        return ToBackend(mAllocation.GetResourceHeap())->GetGPUPointer();
    }

    void Buffer::OnMapCommandSerialFinished(uint32_t mapSerial, void* data, bool isWrite) {
        if (isWrite) {
            CallMapWriteCallback(mapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, data, GetSize());
        } else {
            CallMapReadCallback(mapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, data, GetSize());
        }
    }

    MaybeError Buffer::MapReadAsyncImpl(uint32_t serial) {
        void* data = nullptr;
        DAWN_TRY_ASSIGN(data, mAllocation.Map());

        // There is no need to transition the resource to a new state: D3D12 seems to make the GPU
        // writes available when the fence is passed.
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapRequestTracker();
        tracker->Track(this, serial, static_cast<char*>(data), false);
        return {};
    }

    MaybeError Buffer::MapWriteAsyncImpl(uint32_t serial) {
        void* data = nullptr;
        DAWN_TRY_ASSIGN(data, mAllocation.Map());

        // There is no need to transition the resource to a new state: D3D12 seems to make the CPU
        // writes available on queue submission.
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapRequestTracker();
        tracker->Track(this, serial, data, true);
        return {};
    }

    MaybeError Buffer::UnmapImpl() {
        DAWN_TRY(mAllocation.Unmap());
        return {};
    }

    void Buffer::DestroyImpl() {
        ToBackend(GetDevice())
            ->GetAllocator(mAllocation.GetResourceHeap()->GetHeapTypeIndex(), mAllocatorType)
            ->Deallocate(mAllocation.GetSubAllocationBlock());
        mAllocation = ResourceAllocation();  // Invalidate the allocation handle.
    }

    MapRequestTracker::MapRequestTracker(Device* device) : mDevice(device) {
    }

    MapRequestTracker::~MapRequestTracker() {
        ASSERT(mInflightRequests.Empty());
    }

    void MapRequestTracker::Track(Buffer* buffer, uint32_t mapSerial, void* data, bool isWrite) {
        Request request;
        request.buffer = buffer;
        request.mapSerial = mapSerial;
        request.data = data;
        request.isWrite = isWrite;

        mInflightRequests.Enqueue(std::move(request), mDevice->GetPendingCommandSerial());
    }

    void MapRequestTracker::Tick(Serial finishedSerial) {
        for (auto& request : mInflightRequests.IterateUpTo(finishedSerial)) {
            request.buffer->OnMapCommandSerialFinished(request.mapSerial, request.data,
                                                       request.isWrite);
        }
        mInflightRequests.ClearUpTo(finishedSerial);
    }

}}  // namespace dawn_native::d3d12
