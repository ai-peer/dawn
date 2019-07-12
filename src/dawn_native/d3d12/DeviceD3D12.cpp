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

#include "dawn_native/d3d12/DeviceD3D12.h"

#include "common/Assert.h"
#include "dawn_native/BackendConnection.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/d3d12/AdapterD3D12.h"
#include "dawn_native/d3d12/BackendD3D12.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/BufferD3D12.h"
#include "dawn_native/d3d12/CommandAllocatorManager.h"
#include "dawn_native/d3d12/CommandBufferD3D12.h"
#include "dawn_native/d3d12/ComputePipelineD3D12.h"
#include "dawn_native/d3d12/DescriptorHeapAllocator.h"
#include "dawn_native/d3d12/PipelineLayoutD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
#include "dawn_native/d3d12/QueueD3D12.h"
#include "dawn_native/d3d12/RenderPipelineD3D12.h"
#include "dawn_native/d3d12/ResourceAllocator.h"
#include "dawn_native/d3d12/ResourceAllocatorD3D12.h"
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/ShaderModuleD3D12.h"
#include "dawn_native/d3d12/StagingBufferD3D12.h"
#include "dawn_native/d3d12/SwapChainD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

namespace dawn_native { namespace d3d12 {

    void ASSERT_SUCCESS(HRESULT hr) {
        ASSERT(SUCCEEDED(hr));
    }

    Device::Device(Adapter* adapter, const DeviceDescriptor* descriptor)
        : DeviceBase(adapter, descriptor) {
        if (descriptor != nullptr) {
            ApplyToggleOverrides(descriptor);
        }
    }

    MaybeError Device::Initialize() {
        mD3d12Device = ToBackend(GetAdapter())->GetDevice();

        ASSERT(mD3d12Device != nullptr);

        // Create device-global objects
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ASSERT_SUCCESS(mD3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

        ASSERT_SUCCESS(mD3d12Device->CreateFence(mLastSubmittedSerial, D3D12_FENCE_FLAG_NONE,
                                                 IID_PPV_ARGS(&mFence)));
        mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        ASSERT(mFenceEvent != nullptr);

        // Initialize backend services
        mCommandAllocatorManager = std::make_unique<CommandAllocatorManager>(this);
        mDescriptorHeapAllocator = std::make_unique<DescriptorHeapAllocator>(this);
        mMapRequestTracker = std::make_unique<MapRequestTracker>(this);
        mResourceAllocator = std::make_unique<ResourceAllocator>(this);

        NextSerial();

        // Initialize indirect commands
        D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};
        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC programDesc = {};
        programDesc.ByteStride = 3 * sizeof(uint32_t);
        programDesc.NumArgumentDescs = 1;
        programDesc.pArgumentDescs = &argumentDesc;

        GetD3D12Device()->CreateCommandSignature(&programDesc, NULL,
                                                 IID_PPV_ARGS(&mDispatchIndirectSignature));

        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        programDesc.ByteStride = 4 * sizeof(uint32_t);

        GetD3D12Device()->CreateCommandSignature(&programDesc, NULL,
                                                 IID_PPV_ARGS(&mDrawIndirectSignature));

        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        programDesc.ByteStride = 5 * sizeof(uint32_t);

        GetD3D12Device()->CreateCommandSignature(&programDesc, NULL,
                                                 IID_PPV_ARGS(&mDrawIndexedIndirectSignature));

        return {};
    }

    Device::~Device() {
        // Immediately forget about all pending commands
        if (mPendingCommands.open) {
            mPendingCommands.commandList->Close();
            mPendingCommands.open = false;
            mPendingCommands.commandList = nullptr;
        }
        NextSerial();
        WaitForSerial(mLastSubmittedSerial);  // Wait for all in-flight commands to finish executing
        TickImpl();                    // Call tick one last time so resources are cleaned up

        // Free services explicitly so that they can free D3D12 resources before destruction of the
        // device.
        mDynamicUploader = nullptr;

        // Releasing the uploader enqueues buffers to be released.
        // Call Tick() again to clear them before releasing the allocator.
        mResourceAllocator->Tick(mCompletedSerial);

        // TODO(bryan.bernhart@intel.com): Reuse these heaps rather then release.

        // Release heaps in deletion queue from sub-allocations.
        for (auto& heapTypeAllocator : mResourceAllocators) {
            for (auto& allocatorOfHeapType : heapTypeAllocator.second) {
                for (auto& allocator : allocatorOfHeapType.second) {
                    allocator->Tick(mCompletedSerial);
                }
            }
        }

        // Release heaps in deletion queue from direct allocations
        for (auto& heapTypeAllocator : mDirectResourceAllocators) {
            heapTypeAllocator.second->Tick(mCompletedSerial);
        }

        ASSERT(mUsedComObjectRefs.Empty());
        ASSERT(mPendingCommands.commandList == nullptr);
    }

    ComPtr<ID3D12Device> Device::GetD3D12Device() const {
        return mD3d12Device;
    }

    ComPtr<ID3D12CommandQueue> Device::GetCommandQueue() const {
        return mCommandQueue;
    }

    ComPtr<ID3D12CommandSignature> Device::GetDispatchIndirectSignature() const {
        return mDispatchIndirectSignature;
    }

    ComPtr<ID3D12CommandSignature> Device::GetDrawIndirectSignature() const {
        return mDrawIndirectSignature;
    }

    ComPtr<ID3D12CommandSignature> Device::GetDrawIndexedIndirectSignature() const {
        return mDrawIndexedIndirectSignature;
    }

    DescriptorHeapAllocator* Device::GetDescriptorHeapAllocator() const {
        return mDescriptorHeapAllocator.get();
    }

    ComPtr<IDXGIFactory4> Device::GetFactory() const {
        return ToBackend(GetAdapter())->GetBackend()->GetFactory();
    }

    const PlatformFunctions* Device::GetFunctions() const {
        return ToBackend(GetAdapter())->GetBackend()->GetFunctions();
    }

    MapRequestTracker* Device::GetMapRequestTracker() const {
        return mMapRequestTracker.get();
    }

    ResourceAllocator* Device::GetResourceAllocator() const {
        return mResourceAllocator.get();
    }

    void Device::OpenCommandList(ComPtr<ID3D12GraphicsCommandList>* commandList) {
        ComPtr<ID3D12GraphicsCommandList>& cmdList = *commandList;
        if (!cmdList) {
            ASSERT_SUCCESS(mD3d12Device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                mCommandAllocatorManager->ReserveCommandAllocator().Get(), nullptr,
                IID_PPV_ARGS(&cmdList)));
        } else {
            ASSERT_SUCCESS(
                cmdList->Reset(mCommandAllocatorManager->ReserveCommandAllocator().Get(), nullptr));
        }
    }

    ComPtr<ID3D12GraphicsCommandList> Device::GetPendingCommandList() {
        // Callers of GetPendingCommandList do so to record commands. Only reserve a command
        // allocator when it is needed so we don't submit empty command lists
        if (!mPendingCommands.open) {
            OpenCommandList(&mPendingCommands.commandList);
            mPendingCommands.open = true;
        }
        return mPendingCommands.commandList;
    }

    Serial Device::GetCompletedCommandSerial() const {
        return mCompletedSerial;
    }

    Serial Device::GetLastSubmittedCommandSerial() const {
        return mLastSubmittedSerial;
    }

    Serial Device::GetPendingCommandSerial() const {
        return mLastSubmittedSerial + 1;
    }

    void Device::TickImpl() {
        // Perform cleanup operations to free unused objects
        mCompletedSerial = mFence->GetCompletedValue();

        // Uploader should tick before the resource allocator
        // as it enqueues resources to be released.
        mDynamicUploader->Tick(mCompletedSerial);

        mResourceAllocator->Tick(mCompletedSerial);

        // Release heaps in deletion queue from sub-allocations.
        for (auto& heapTypeAllocator : mResourceAllocators) {
            for (auto& allocatorOfHeapType : heapTypeAllocator.second) {
                for (auto& allocator : allocatorOfHeapType.second) {
                    allocator->Tick(mCompletedSerial);
                }
            }
        }

        // Release heaps in deletion queue from direct allocations
        for (auto& heapTypeAllocator : mDirectResourceAllocators) {
            heapTypeAllocator.second->Tick(mCompletedSerial);
        }

        mCommandAllocatorManager->Tick(mCompletedSerial);
        mDescriptorHeapAllocator->Tick(mCompletedSerial);
        mMapRequestTracker->Tick(mCompletedSerial);
        mUsedComObjectRefs.ClearUpTo(mCompletedSerial);
        ExecuteCommandLists({});
        NextSerial();
    }

    void Device::NextSerial() {
        mLastSubmittedSerial++;
        ASSERT_SUCCESS(mCommandQueue->Signal(mFence.Get(), mLastSubmittedSerial));
    }

    void Device::WaitForSerial(uint64_t serial) {
        mCompletedSerial = mFence->GetCompletedValue();
        if (mCompletedSerial < serial) {
            ASSERT_SUCCESS(mFence->SetEventOnCompletion(serial, mFenceEvent));
            WaitForSingleObject(mFenceEvent, INFINITE);
        }
    }

    void Device::ReferenceUntilUnused(ComPtr<IUnknown> object) {
        mUsedComObjectRefs.Enqueue(object, GetPendingCommandSerial());
    }

    void Device::ExecuteCommandLists(std::initializer_list<ID3D12CommandList*> commandLists) {
        // If there are pending commands, prepend them to ExecuteCommandLists
        if (mPendingCommands.open) {
            std::vector<ID3D12CommandList*> lists(commandLists.size() + 1);
            mPendingCommands.commandList->Close();
            mPendingCommands.open = false;
            lists[0] = mPendingCommands.commandList.Get();
            std::copy(commandLists.begin(), commandLists.end(), lists.begin() + 1);
            mCommandQueue->ExecuteCommandLists(static_cast<UINT>(commandLists.size() + 1),
                                               lists.data());
            mPendingCommands.commandList = nullptr;
        } else {
            std::vector<ID3D12CommandList*> lists(commandLists);
            mCommandQueue->ExecuteCommandLists(static_cast<UINT>(commandLists.size()),
                                               lists.data());
        }
    }

    ResultOrError<BindGroupBase*> Device::CreateBindGroupImpl(
        const BindGroupDescriptor* descriptor) {
        return new BindGroup(this, descriptor);
    }
    ResultOrError<BindGroupLayoutBase*> Device::CreateBindGroupLayoutImpl(
        const BindGroupLayoutDescriptor* descriptor) {
        return new BindGroupLayout(this, descriptor);
    }
    ResultOrError<BufferBase*> Device::CreateBufferImpl(const BufferDescriptor* descriptor) {
        return new Buffer(this, descriptor);
    }
    CommandBufferBase* Device::CreateCommandBuffer(CommandEncoderBase* encoder,
                                                   const CommandBufferDescriptor* descriptor) {
        return new CommandBuffer(encoder, descriptor);
    }
    ResultOrError<ComputePipelineBase*> Device::CreateComputePipelineImpl(
        const ComputePipelineDescriptor* descriptor) {
        return new ComputePipeline(this, descriptor);
    }
    ResultOrError<PipelineLayoutBase*> Device::CreatePipelineLayoutImpl(
        const PipelineLayoutDescriptor* descriptor) {
        return new PipelineLayout(this, descriptor);
    }
    ResultOrError<QueueBase*> Device::CreateQueueImpl() {
        return new Queue(this);
    }
    ResultOrError<RenderPipelineBase*> Device::CreateRenderPipelineImpl(
        const RenderPipelineDescriptor* descriptor) {
        return new RenderPipeline(this, descriptor);
    }
    ResultOrError<SamplerBase*> Device::CreateSamplerImpl(const SamplerDescriptor* descriptor) {
        return new Sampler(this, descriptor);
    }
    ResultOrError<ShaderModuleBase*> Device::CreateShaderModuleImpl(
        const ShaderModuleDescriptor* descriptor) {
        return new ShaderModule(this, descriptor);
    }
    ResultOrError<SwapChainBase*> Device::CreateSwapChainImpl(
        const SwapChainDescriptor* descriptor) {
        return new SwapChain(this, descriptor);
    }
    ResultOrError<TextureBase*> Device::CreateTextureImpl(const TextureDescriptor* descriptor) {
        return new Texture(this, descriptor);
    }
    ResultOrError<TextureViewBase*> Device::CreateTextureViewImpl(
        TextureBase* texture,
        const TextureViewDescriptor* descriptor) {
        return new TextureView(texture, descriptor);
    }

    ResultOrError<std::unique_ptr<StagingBufferBase>> Device::CreateStagingBuffer(size_t size) {
        std::unique_ptr<StagingBufferBase> stagingBuffer =
            std::make_unique<StagingBuffer>(size, this);
        return std::move(stagingBuffer);
    }

    MaybeError Device::CopyFromStagingToBuffer(StagingBufferBase* source,
                                               uint64_t sourceOffset,
                                               BufferBase* destination,
                                               uint64_t destinationOffset,
                                               uint64_t size) {
        ToBackend(destination)
            ->TransitionUsageNow(GetPendingCommandList(), dawn::BufferUsageBit::CopyDst);

        GetPendingCommandList()->CopyBufferRegion(
            ToBackend(destination)->GetD3D12Resource().Get(), destinationOffset,
            ToBackend(source)->GetResource(), sourceOffset, size);

        return {};
    }

    // Create the sub-allocators which allocate resource heaps of power-of-two sizes.
    std::vector<std::unique_ptr<PlacedResourceAllocator>> Device::CreateResourceAllocators(
        D3D12_HEAP_TYPE heapType) {
        // One approach is a create a list of these heaps of various sizes (ie. linear pool), but
        // this strategy has two issues: 1) a seperate allocator instance is required to manage
        // every heap no matter the size and 2) the largest heap would always stay resident (or
        // pinned) preventing smaller heaps from being reused.
        //
        // A better strategy would be to align the heap size to a power-of-two then get the
        // corresponding allocator by computing the 2^index or level. Then only
        // Log2(MaxBlockSize) allocators ever exist and smaller heaps can be reused by
        // specifying a smaller level.
        std::vector<std::unique_ptr<PlacedResourceAllocator>> allocators;
        for (size_t resourceHeapSize = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
             resourceHeapSize <= kMaxResourceSize; resourceHeapSize *= 2) {
            std::unique_ptr<PlacedResourceAllocator> allocator =
                std::make_unique<PlacedResourceAllocator>(kMaxResourceSize, resourceHeapSize, this,
                                                          heapType);
            allocators.push_back(std::move(allocator));
        }
        return allocators;
    }

    // Helper to compute the index of which allocator to use that has a heap large enough to satisfy
    // the allocation request. Needed by [Allocate|Deallocate]Memory.
    size_t Device::ComputeLevelFromHeapSize(size_t heapSize) const {
        return Log2(heapSize) - Log2(D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
    }

    void Device::DeallocateMemory(ResourceMemoryAllocation& allocation, D3D12_HEAP_TYPE heapType) {
        PlacedResourceAllocator* allocator = nullptr;
        if (allocation.IsDirect()) {
            allocator = mDirectResourceAllocators[heapType].get();
        } else {
            const D3D12_HEAP_DESC heapInfo =
                ToBackend(allocation.GetResourceHeap())->GetD3D12Heap()->GetDesc();
            const size_t heapLevel = ComputeLevelFromHeapSize(heapInfo.SizeInBytes);

            allocator = mResourceAllocators[heapInfo.Flags][heapType][heapLevel].get();
        }
        allocator->Deallocate(allocation);
    }

    ResultOrError<ResourceMemoryAllocation> Device::AllocateMemory(
        D3D12_HEAP_TYPE heapType,
        D3D12_RESOURCE_DESC resourceDescriptor,
        D3D12_HEAP_FLAGS heapFlags) {
        const D3D12_RESOURCE_ALLOCATION_INFO resourceInfo =
            GetD3D12Device()->GetResourceAllocationInfo(0, 1, &resourceDescriptor);

        // TODO(bryan.bernhart@intel.com): Dynamically disable sub-allocation.
        // For very large resources, there is no beneifit to sub-allocate them from a larger heap
        // and would otherwise increase internal fragementation (due to power-of-two).
        //
        // For very small resources, it is inefficent to sub-allocate them since the min. heap size
        // or page-size is 64KB.
        //
        // This decision could be determined at allocation-time or when a budget event fires.
        bool isDirect = true;

        PlacedResourceAllocator* allocator = nullptr;
        uint64_t allocationSize = resourceInfo.SizeInBytes;

        if (isDirect) {
            // Get the direct allocator using a tightly sized heap (aka CreateCommittedResource).
            ASSERT(IsAligned(resourceInfo.SizeInBytes, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));

            if (mDirectResourceAllocators.find(heapType) == mDirectResourceAllocators.end()) {
                mDirectResourceAllocators[heapType] =
                    std::make_unique<PlacedResourceAllocator>(this, heapType);
            }

            allocator = mDirectResourceAllocators[heapType].get();
        } else {
            // PlacedResourceAllocator (aka CreateHeap) requires heap flags to be explicitly
            // specified. However, not all GPUs allow mixed resource types to co-exist on the same
            // physical heap nor does PlacedResourceAllocator allow sub-allocation with multiple
            // heap options. Instead, a seperate set of allocators (per heap flag) is needed.
            if (mResourceAllocators.find(heapFlags) == mResourceAllocators.end()) {
                std::map<D3D12_HEAP_TYPE, SubAllocatorPool> allocators;
                allocators.insert(std::pair<D3D12_HEAP_TYPE, SubAllocatorPool>(
                    heapType, CreateResourceAllocators(heapType)));

                mResourceAllocators.insert(
                    std::pair<D3D12_HEAP_FLAGS, std::map<D3D12_HEAP_TYPE, SubAllocatorPool>>(
                        heapFlags, std::move(allocators)));
            } else if (mResourceAllocators[heapFlags].find(heapType) ==
                       mResourceAllocators[heapFlags].end()) {
                mResourceAllocators[heapFlags].insert(std::pair<D3D12_HEAP_TYPE, SubAllocatorPool>(
                    heapType, CreateResourceAllocators(heapType)));
            }

            // SubAllocations must be power-of-two aligned.
            allocationSize = (IsPowerOfTwo(allocationSize)) ? allocationSize
                                                            : AlignToNextPowerOfTwo(allocationSize);

            // TODO(bryan.bernhart@intel.com): Adjust the heap size based on
            // a heuristic. Smaller but frequent allocations benefit by sub-allocating from a larger
            // heap.
            const size_t heapSize = allocationSize;
            const size_t heapLevel = ComputeLevelFromHeapSize(heapSize);

            allocator = mResourceAllocators[heapFlags][heapType][heapLevel].get();
        }

        ResourceMemoryAllocation allocation =
            allocator->Allocate(resourceDescriptor, allocationSize, heapFlags);

        // Device lost or OOM.
        if (allocation.GetOffset() == INVALID_OFFSET) {
            return DAWN_CONTEXT_LOST_ERROR("Unable to allocate memory for resource");
        }

        return ResourceMemoryAllocation{allocation.GetOffset(), allocation.GetResourceHeap(),
                                        isDirect};
    }
}}  // namespace dawn_native::d3d12
