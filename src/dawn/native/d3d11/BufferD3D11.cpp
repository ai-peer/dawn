// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d11/BufferD3D11.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {
namespace {

// Some D3D11 limitations:
// - D3D11 doesn't support constants buffers with other accelerated GPU usages.
// - D3D11 doesn't allow GPU copy data to a mapped destination buffer.
// - D3D11 doesn't allow copying between buffer and texture.
// - UpdateSubresource() can't partially update constant buffers.

constexpr wgpu::BufferUsage kMapUsages = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::MapWrite;

MaybeError ValidationUsage(wgpu::BufferUsage usage) {
    // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_bind_flag
    // D3D11 doesn't support constants buffers with other accelerated GPU usages.
    // TODO(dawn:1755): find a way to workaround this D3D11 limitation.
    constexpr wgpu::BufferUsage kAllowedUniformBufferUsages =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;

    DAWN_INVALID_IF(
        usage & wgpu::BufferUsage::Uniform && !IsSubset(usage, kAllowedUniformBufferUsages),
        "Buffer usage can't be both uniform and other accelerated usages with D3D11");

    return {};
}

bool IsMapUsage(wgpu::BufferUsage usage) {
    return usage & kMapUsages;
}

D3D11_USAGE D3D11BufferUsage(wgpu::BufferUsage usage) {
    if (IsMapUsage(usage)) {
        return D3D11_USAGE_STAGING;
    } else {
        return D3D11_USAGE_DEFAULT;
    }
}

UINT D3D11BufferBindFlags(wgpu::BufferUsage usage) {
    UINT bindFlags = 0;

    if (usage & (wgpu::BufferUsage::Vertex)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
    }
    if (usage & wgpu::BufferUsage::Index) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_INDEX_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Uniform)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_UNORDERED_ACCESS;
    }
    if (usage & kReadOnlyStorageBuffer) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
    }

    return bindFlags;
}

UINT D3D11CpuAccessFlags(wgpu::BufferUsage usage) {
    UINT cpuAccessFlags = 0;
    if (IsMapUsage(usage)) {
        // D3D11 doesn't allow copying between buffer and texture.
        //  - For buffer to texture copy, we need to use a staging(mappable) texture, and memcpy the
        //    data from the staging buffer to the staging texture first. So D3D11_CPU_ACCESS_READ is
        //    needed for MapWrite usage.
        //  - For texture to buffer copy, we may need copy texture to a staging (mappable)
        //    texture, and then memcpy the data from the staging texture to the staging buffer. So
        //    D3D11_CPU_ACCESS_WRITE is needed to MapRead usage.
        cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ |
                         D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
    }
    return cpuAccessFlags;
}

UINT D3D11BufferMiscFlags(wgpu::BufferUsage usage) {
    UINT miscFlags = 0;
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        miscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }
    if (usage & wgpu::BufferUsage::Indirect) {
        miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    return miscFlags;
}

size_t D3D11BufferSizeAlignment(wgpu::BufferUsage usage) {
    if (usage & wgpu::BufferUsage::Uniform) {
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1
        // Each number of constants must be a multiple of 16 shader constants(sizeof(float) * 4 *
        // 16).
        return sizeof(float) * 4 * 16;
    }

    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        // Unordered access buffers must be 4-byte aligned.
        return sizeof(uint32_t);
    }
    return 1;
}

}  // namespace

// static
ResultOrError<Ref<Buffer>> Buffer::Create(Device* device, const BufferDescriptor* descriptor) {
    Ref<Buffer> buffer = AcquireRef(new Buffer(device, descriptor));
    DAWN_TRY(buffer->Initialize(descriptor->mappedAtCreation));
    return buffer;
}

Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
    : BufferBase(device, descriptor) {}

MaybeError Buffer::Initialize(bool mappedAtCreation) {
    DAWN_TRY(ValidationUsage(GetUsage()));

    // Allocate at least 4 bytes so clamped accesses are always in bounds.
    uint64_t size = std::max(GetSize(), uint64_t(4u));
    size_t alignment = D3D11BufferSizeAlignment(GetUsage());
    if (size > std::numeric_limits<uint64_t>::max() - alignment) {
        // Alignment would overlow.
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }
    mAllocatedSize = Align(size, alignment);

    // Create mD3d11Buffer
    D3D11_BUFFER_DESC bufferDescriptor;
    bufferDescriptor.ByteWidth = mAllocatedSize;
    bufferDescriptor.Usage = D3D11BufferUsage(GetUsage());
    bufferDescriptor.BindFlags = D3D11BufferBindFlags(GetUsage());
    bufferDescriptor.CPUAccessFlags = D3D11CpuAccessFlags(GetUsage());
    bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(GetUsage());
    bufferDescriptor.StructureByteStride = 0;

    DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                              ->GetD3D11Device()
                              ->CreateBuffer(&bufferDescriptor, nullptr, &mD3d11Buffer),
                          "ID3D11Device::CreateBuffer"));

    // D3D11 doesn't allow mapped buffer as copy destination, so we cannot keep the buffer mapped
    // all the time.
    if ((GetUsage() & kMapUsages) == wgpu::BufferUsage::MapWrite) {
        DAWN_TRY(MapInternal("Buffer::Initialize"));
    }

    SetLabelImpl();
    return {};
}

Buffer::~Buffer() = default;

bool Buffer::IsCPUWritableAtCreation() const {
    return IsMapUsage(GetUsage());
}

MaybeError Buffer::MapInternal(const char* contextInfo) {
    DAWN_ASSERT(IsMapUsage(GetUsage()));
    DAWN_ASSERT(!mMappedData);

    CommandRecordingContext* commandContext;
    DAWN_TRY_ASSIGN(commandContext, ToBackend(GetDevice())->GetPendingCommandContext());

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    DAWN_TRY(CheckHRESULT(commandContext->GetD3D11DeviceContext()->Map(
                              mD3d11Buffer.Get(), /*Subresource=*/0, D3D11_MAP_READ_WRITE,
                              /*MapFlags=*/0, &mappedResource),
                          contextInfo));
    mMappedData = reinterpret_cast<uint8_t*>(mappedResource.pData);

    return {};
}

void Buffer::UnmapInternal() {
    DAWN_ASSERT(mMappedData);

    auto result = ToBackend(GetDevice())->GetPendingCommandContext();
    DAWN_ASSERT(result.IsSuccess());
    CommandRecordingContext* commandContext = result.AcquireSuccess();
    commandContext->GetD3D11DeviceContext()->Unmap(mD3d11Buffer.Get(), /*Subresource=*/0);

    mMappedData = nullptr;
}

MaybeError Buffer::MapAtCreationImpl() {
    DAWN_ASSERT(IsMapUsage(GetUsage()));

    if (mMappedData) {
        DAWN_ASSERT((GetUsage() & kMapUsages) == wgpu::BufferUsage::MapWrite);
        return {};
    }

    DAWN_ASSERT((GetUsage() & kMapUsages) == wgpu::BufferUsage::MapRead);
    return MapInternal("Bufer::MapAtCreationImpl");
}

MaybeError Buffer::MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) {
    DAWN_ASSERT(mD3d11Buffer);

    if (!mMappedData) {
        DAWN_ASSERT((GetUsage() & kMapUsages) == wgpu::BufferUsage::MapRead);
        DAWN_TRY(MapInternal("Buffer::MapAsyncImpl"));
    }

    // Do not need pass a commandContext to EnsureDataInitialized because the staging buffer is
    // mapped, and the data will be initialized by using memset()
    DAWN_TRY(EnsureDataInitialized(/*commandContext=*/nullptr));

    return {};
}

void Buffer::UnmapImpl() {
    DAWN_ASSERT(mD3d11Buffer);
    DAWN_ASSERT(mMappedData);

    // D3D11 doesn't allow mapped buffer as copy destination, so we cannot keep the buffer mapped
    // all the time.
    if ((GetUsage() & kMapUsages) == wgpu::BufferUsage::MapRead) {
        UnmapInternal();
    }
}

void* Buffer::GetMappedPointer() {
    // The frontend asks that the pointer returned is from the start of the resource
    // irrespective of the offset passed in MapAsyncImpl, which is what mMappedData is.
    return mMappedData;
}

void Buffer::DestroyImpl() {
    BufferBase::DestroyImpl();
    if (mMappedData) {
        UnmapInternal();
    }
    mD3d11Buffer = nullptr;
}

MaybeError Buffer::EnsureDataInitialized(CommandRecordingContext* commandContext) {
    if (!NeedsInitialization()) {
        return {};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {};
}

ResultOrError<bool> Buffer::EnsureDataInitializedAsDestination(
    CommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    if (!NeedsInitialization()) {
        return {false};
    }

    if (IsFullBufferRange(offset, size)) {
        SetIsDataInitialized();
        return {false};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {true};
}

MaybeError Buffer::EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                      const CopyTextureToBufferCmd* copy) {
    if (!NeedsInitialization()) {
        return {};
    }

    if (IsFullBufferOverwrittenInTextureToBufferCopy(copy)) {
        SetIsDataInitialized();
    } else {
        DAWN_TRY(InitializeToZero(commandContext));
    }

    return {};
}

void Buffer::SetLabelImpl() {
    SetDebugName(ToBackend(GetDevice()), mD3d11Buffer.Get(), "Dawn_Buffer", GetLabel());
}

MaybeError Buffer::InitializeToZero(CommandRecordingContext* commandContext) {
    DAWN_ASSERT(NeedsInitialization());

    // TODO(crbug.com/dawn/484): skip initializing the buffer when it is created on a heap
    // that has already been zero initialized.
    DAWN_TRY(ClearInternal(commandContext, uint8_t(0u)));
    SetIsDataInitialized();
    GetDevice()->IncrementLazyClearCountForTesting();

    return {};
}

ResultOrError<ComPtr<ID3D11ShaderResourceView>> Buffer::CreateD3D11ShaderResourceView(
    uint64_t offset,
    uint64_t size) const {
    DAWN_ASSERT(IsAligned(offset, 4u));
    DAWN_ASSERT(IsAligned(size, 4u));
    UINT firstElement = static_cast<UINT>(offset / 4);
    UINT numElements = static_cast<UINT>(size / 4);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;

    ComPtr<ID3D11ShaderResourceView> srv;
    DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                              ->GetD3D11Device()
                              ->CreateShaderResourceView(mD3d11Buffer.Get(), &desc, &srv),
                          "ShaderResourceView creation"));

    return srv;
}

ResultOrError<ComPtr<ID3D11UnorderedAccessView1>> Buffer::CreateD3D11UnorderedAccessView1(
    uint64_t offset,
    uint64_t size) const {
    DAWN_ASSERT(IsAligned(offset, 4u));
    DAWN_ASSERT(IsAligned(size, 4u));

    UINT firstElement = static_cast<UINT>(offset / 4);
    UINT numElements = static_cast<UINT>(size / 4);

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 desc;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;
    desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    ComPtr<ID3D11UnorderedAccessView1> uav;
    DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                              ->GetD3D11Device5()
                              ->CreateUnorderedAccessView1(mD3d11Buffer.Get(), &desc, &uav),
                          "UnorderedAccessView creation"));

    return uav;
}

MaybeError Buffer::Clear(CommandRecordingContext* commandContext,
                         uint8_t clearValue,
                         uint64_t offset,
                         uint64_t size) {
    DAWN_ASSERT(!mMappedData);

    if (size == 0) {
        return {};
    }

    // For non-staging buffers, we can use UpdateSubresource to write the data.
    DAWN_TRY_ASSIGN(std::ignore,
                    this->EnsureDataInitializedAsDestination(commandContext, offset, size));
    return this->ClearInternal(commandContext, clearValue, offset, size);
}

MaybeError Buffer::ClearInternal(CommandRecordingContext* commandContext,
                                 uint8_t clearValue,
                                 uint64_t offset,
                                 uint64_t size) {
    if (size <= 0) {
        DAWN_ASSERT(offset == 0);
        size = GetAllocatedSize();
    }

    if (mMappedData) {
        memset(mMappedData + offset, clearValue, size);
        return {};
    }

    // TODO(dawn:1705): use a reusable zero staging buffer to clear the buffer to avoid this CPU to
    // GPU copy.
    std::vector<uint8_t> clearData(size, clearValue);
    return this->WriteInternal(commandContext, offset, clearData.data(), size);
}

MaybeError Buffer::Write(CommandRecordingContext* commandContext,
                         uint64_t offset,
                         const void* data,
                         size_t size) {
    if (size == 0) {
        return {};
    }

    // For non-staging buffers, we can use UpdateSubresource to write the data.
    DAWN_TRY_ASSIGN(std::ignore,
                    this->EnsureDataInitializedAsDestination(commandContext, offset, size));
    return this->WriteInternal(commandContext, offset, data, size);
}

MaybeError Buffer::WriteInternal(CommandRecordingContext* commandContext,
                                 uint64_t offset,
                                 const void* data,
                                 size_t size) {
    if (size == 0) {
        return {};
    }

    if (mMappedData) {
        memcpy(mMappedData + offset, data, size);
        return {};
    }

    ID3D11DeviceContext1* d3d11DeviceContext1 = commandContext->GetD3D11DeviceContext1();
    if (this->GetUsage() & wgpu::BufferUsage::Uniform) {
        if (offset == 0 && size == GetAllocatedSize()) {
            d3d11DeviceContext1->UpdateSubresource(this->GetD3D11Buffer(), 0, nullptr, data, 0, 0);
            return {};
        }

        Device* device = ToBackend(GetDevice());
        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                          size, device->GetPendingCommandSerial(),
                                          kCopyBufferToBufferOffsetAlignment));
        DAWN_ASSERT(uploadHandle.mappedBuffer != nullptr);
        memcpy(uploadHandle.mappedBuffer, data, size);

        D3D11_BOX srcBox;
        srcBox.left = uploadHandle.startOffset;
        srcBox.right = uploadHandle.startOffset + size;
        srcBox.top = 0;
        srcBox.bottom = 1;
        srcBox.front = 0;
        srcBox.back = 1;
        d3d11DeviceContext1->CopySubresourceRegion(
            this->GetD3D11Buffer(), 0, offset, 0, 0,
            ToBackend(uploadHandle.stagingBuffer)->GetD3D11Buffer(), 0, &srcBox);

        return {};
    }

    D3D11_BOX dstBox;
    dstBox.left = static_cast<UINT>(offset);
    dstBox.right = static_cast<UINT>(offset + size);
    dstBox.top = 0;
    dstBox.bottom = 1;
    dstBox.front = 0;
    dstBox.back = 1;

    d3d11DeviceContext1->UpdateSubresource(this->GetD3D11Buffer(), 0, &dstBox, data, 0, 0);
    return {};
}

MaybeError Buffer::CopyFromBuffer(CommandRecordingContext* commandContext,
                                  uint64_t offset,
                                  size_t size,
                                  Buffer* source,
                                  uint64_t sourceOffset) {
    if (size == 0) {
        // Skip no-op copies.
        return {};
    }

    DAWN_TRY(source->EnsureDataInitialized(commandContext));
    DAWN_TRY_ASSIGN(std::ignore,
                    this->EnsureDataInitializedAsDestination(commandContext, offset, size));

    D3D11_BOX srcBox;
    srcBox.left = sourceOffset;
    srcBox.right = sourceOffset + size;
    srcBox.top = 0;
    srcBox.bottom = 1;
    srcBox.front = 0;
    srcBox.back = 1;
    commandContext->GetD3D11DeviceContext()->CopySubresourceRegion(
        this->GetD3D11Buffer(), 0, offset, 0, 0, source->GetD3D11Buffer(), 0, &srcBox);
    return {};
}

uint8_t* Buffer::GetMappedData() const {
    return mMappedData;
}

}  // namespace dawn::native::d3d11
