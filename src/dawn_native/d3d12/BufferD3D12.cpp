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
        D3D12_RESOURCE_FLAGS D3D12ResourceFlags(dawn::BufferUsageBit usage) {
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

            if (usage & dawn::BufferUsageBit::Storage) {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            return flags;
        }

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
            if (usage & dawn::BufferUsageBit::Indirect) {
                resourceState |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
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
    }  // namespace

    Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
        : BufferBase(device, descriptor) {
        if (GetDevice()->ConsumedError(Initialize())) {
            return;
        }
    }

    MaybeError Buffer::Initialize() {
        D3D12_RESOURCE_DESC resourceDescriptor;
        resourceDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDescriptor.Alignment = 0;
        resourceDescriptor.Width = GetD3D12Size();
        resourceDescriptor.Height = 1;
        resourceDescriptor.DepthOrArraySize = 1;
        resourceDescriptor.MipLevels = 1;
        resourceDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        resourceDescriptor.SampleDesc.Count = 1;
        resourceDescriptor.SampleDesc.Quality = 0;
        resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        // Add TransferDst for non-mappable buffer initialization in CreateBufferMapped
        // and robust resource initialization.
        resourceDescriptor.Flags =
            D3D12ResourceFlags(GetUsage() | dawn::BufferUsageBit::TransferDst);

        auto heapType = D3D12HeapType(GetUsage());

        // D3D12 requires buffers on the READBACK heap to have the D3D12_RESOURCE_STATE_COPY_DEST
        // state
        if (heapType == D3D12_HEAP_TYPE_READBACK) {
            mFixedResourceState = true;
            mLastUsage = dawn::BufferUsageBit::TransferDst;
        }

        // D3D12 requires buffers on the UPLOAD heap to have the D3D12_RESOURCE_STATE_GENERIC_READ
        // state
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            mFixedResourceState = true;
            mLastUsage = dawn::BufferUsageBit::TransferSrc;
        }

        DAWN_TRY_ASSIGN(mAllocation, ToBackend(GetDevice())
                                         ->AllocateMemory(heapType, resourceDescriptor,
                                                          D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS));
        return {};
    }

    Buffer::~Buffer() {
        DestroyInternal();
    }

    bool Buffer::CreateD3D12ResourceBarrierIfNeeded(D3D12_RESOURCE_BARRIER* barrier,
                                                    dawn::BufferUsageBit newUsage) const {
        // Resources in upload and readback heaps must be kept in the COPY_SOURCE/DEST state
        if (mFixedResourceState) {
            ASSERT(mLastUsage == newUsage);
            return false;
        }

        // We can skip transitions to already current usages.
        // TODO(cwallez@chromium.org): Need some form of UAV barriers at some point.
        if ((mLastUsage & newUsage) == newUsage) {
            return false;
        }

        D3D12_RESOURCE_STATES lastState = D3D12BufferUsage(mLastUsage);
        D3D12_RESOURCE_STATES newState = D3D12BufferUsage(newUsage);

        // The COMMON state represents a state where no write operations can be pending, which makes
        // it possible to transition to some states without synchronizaton (i.e. without an explicit
        // ResourceBarrier call). This can be to 1) a single write state, or 2) multiple read
        // states.
        //
        // Destination states used in Dawn that qualify for implicit transition for a buffer:
        // COPY_SOURCE, VERTEX_AND_CONSTANT_BUFFER, INDEX_BUFFER, COPY_DEST, UNORDERED_ACCESS
        // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#implicit-state-transitions
        {
            static constexpr D3D12_RESOURCE_STATES kD3D12BufferReadOnlyStates =
                D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
                D3D12_RESOURCE_STATE_INDEX_BUFFER;

            if (lastState == D3D12_RESOURCE_STATE_COMMON) {
                bool singleWriteState = ((newState == D3D12_RESOURCE_STATE_COPY_DEST) ||
                                         (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
                bool readOnlyState = newState == (newState & kD3D12BufferReadOnlyStates);
                if (singleWriteState ^ readOnlyState) {
                    return false;
                }
            }
        }

        barrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier->Transition.pResource = GetD3D12Resource().Get();
        barrier->Transition.StateBefore = lastState;
        barrier->Transition.StateAfter = newState;
        barrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        return true;
    }

    uint32_t Buffer::GetD3D12Size() const {
        // TODO(enga@google.com): TODO investigate if this needs to be a constraint at the API level
        return Align(GetSize(), 256);
    }

    ComPtr<ID3D12Resource> Buffer::GetD3D12Resource() const {
        return ToBackend(mAllocation.GetResourceHeap())->GetD3D12Resource();
    }

    void Buffer::SetUsage(dawn::BufferUsageBit newUsage) {
        mLastUsage = newUsage;
    }

    void Buffer::TransitionUsageNow(ComPtr<ID3D12GraphicsCommandList> commandList,
                                    dawn::BufferUsageBit usage) {
        D3D12_RESOURCE_BARRIER transitionBarrier;

        if (CreateD3D12ResourceBarrierIfNeeded(&transitionBarrier, usage)) {
            commandList->ResourceBarrier(1, &transitionBarrier);
        }

        D3D12_RESOURCE_BARRIER aliasingBarrier;
        aliasingBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        aliasingBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        // Deactivate all other resources mapped to the same physical resource heap.
        // Note: pResourceBefore tells the driver to optimize out the temporary overlap
        // during the aliasing transition. Resource allocations (placed resource) are disjoint and
        // there is no overlap in the physical heap space. But the same physical heap space may get
        // reallocated with a new placed resource. In that case, only the previous resource needs to
        // be specified for this transition. In addition, placed resources on two seperate
        // commandLists do not require a aliasing transition at all.
        // TODO(bryan.bernhart@intel.com): Optimize out these cases of aliasing transitions.
        aliasingBarrier.Aliasing.pResourceBefore = nullptr;
        aliasingBarrier.Aliasing.pResourceAfter = GetD3D12Resource().Get();

        commandList->ResourceBarrier(1, &aliasingBarrier);

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

    bool Buffer::IsMapWritable() const {
        // TODO(enga): Handle CPU-visible memory on UMA
        return (GetUsage() & (dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::MapWrite)) != 0;
    }

    MaybeError Buffer::MapAtCreationImpl(uint8_t** mappedPointer) {
        void* data = nullptr;
        DAWN_TRY_ASSIGN(data, mAllocation.GetResourceHeap()->Map());
        *mappedPointer = static_cast<uint8_t*>(data);
        return {};
    }

    MaybeError Buffer::MapReadAsyncImpl(uint32_t serial) {
        void* data = nullptr;
        DAWN_TRY_ASSIGN(data, mAllocation.GetResourceHeap()->Map());

        // There is no need to transition the resource to a new state: D3D12 seems to make the GPU
        // writes available when the fence is passed.
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapRequestTracker();
        tracker->Track(this, serial, static_cast<char*>(data), false);
        return {};
    }

    MaybeError Buffer::MapWriteAsyncImpl(uint32_t serial) {
        void* data = nullptr;
        DAWN_TRY_ASSIGN(data, mAllocation.GetResourceHeap()->Map());

        // There is no need to transition the resource to a new state: D3D12 seems to make the CPU
        // writes available on queue submission.
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapRequestTracker();
        tracker->Track(this, serial, data, true);
        return {};
    }

    void Buffer::UnmapImpl() {
        mAllocation.GetResourceHeap()->Unmap();
    }

    void Buffer::DestroyImpl() {
        ToBackend(GetDevice())->DeallocateMemory(mAllocation, D3D12HeapType(GetUsage()));
        mAllocation = ResourceMemoryAllocation();  // Invalidate the allocation handle.
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
