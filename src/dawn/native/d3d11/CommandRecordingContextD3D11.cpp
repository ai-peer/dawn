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

#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"

#include <string>
#include <utility>

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/Forward.h"
#include "dawn/native/d3d11/PipelineLayoutD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

MaybeError CommandRecordingContext::Intialize(Device* device) {
    DAWN_ASSERT(!IsOpen());
    DAWN_ASSERT(device);
    mDevice = device;
    mNeedsSubmit = false;

    ID3D11Device* d3d11Device = device->GetD3D11Device();

    ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
    device->GetD3D11Device()->GetImmediateContext(&d3d11DeviceContext);

    ComPtr<ID3D11DeviceContext4> d3d11DeviceContext4;
    DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&d3d11DeviceContext4),
                          "D3D11 querying immediate context for ID3D11DeviceContext4 interface"));

    DAWN_TRY(
        CheckHRESULT(d3d11DeviceContext4.As(&mD3dUserDefinedAnnotation),
                     "D3D11 querying immediate context for ID3DUserDefinedAnnotation interface"));

    if (device->HasFeature(Feature::D3D11MultithreadProtected)) {
        DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&mD3d11Multithread),
                              "D3D11 querying immediate context for ID3D11Multithread interface"));
        mD3d11Multithread->SetMultithreadProtected(TRUE);
    }

    mD3d11Device = d3d11Device;
    mD3d11DeviceContext4 = std::move(d3d11DeviceContext4);
    mIsOpen = true;

    // Create a uniform buffer for indirect draw/dispatch args.
    BufferDescriptor descriptor;
    descriptor.size = sizeof(uint32_t) * kMaxNumBuiltinElements;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    descriptor.mappedAtCreation = false;
    descriptor.label = "builtin uniform buffer";

    Ref<BufferBase> uniformBuffer;
    {
        // Lock the device to protect the clearing of the built-in uniform buffer.
        auto deviceLock(device->GetScopedLock());
        DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&descriptor));
    }
    mIndirectUniformBuffer = ToBackend(std::move(uniformBuffer));

    // Create a d3d11 buffer of dynamic usage for built-in variables.
    {
        D3D11_BUFFER_DESC bufferDescriptor;
        bufferDescriptor.ByteWidth =
            Align(descriptor.size, Buffer::D3D11BufferSizeAlignment(wgpu::BufferUsage::Uniform));
        bufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
        bufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDescriptor.MiscFlags = 0;
        bufferDescriptor.StructureByteStride = 0;

        DAWN_TRY(CheckOutOfMemoryHRESULT(
            GetD3D11Device()->CreateBuffer(&bufferDescriptor, nullptr, &mD3d11UniformBuffer),
            "ID3D11Device::CreateBuffer"));
    }

    // Bind the dynamic built-in uniform buffer.
    {
        auto deviceLock(device->GetScopedLock());
        mIsLastDrawOrDisplayIndirect = true;
        OnDrawOrDispatch(/*indirect=*/false);
    }
    return {};
}

MaybeError CommandRecordingContext::ExecuteCommandList(Device* device) {
    // Consider using deferred DeviceContext.
    mNeedsSubmit = false;
    return {};
}

ID3D11Device* CommandRecordingContext::GetD3D11Device() const {
    return mD3d11Device.Get();
}

ID3D11DeviceContext* CommandRecordingContext::GetD3D11DeviceContext() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3d11DeviceContext4.Get();
}

ID3D11DeviceContext1* CommandRecordingContext::GetD3D11DeviceContext1() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3d11DeviceContext4.Get();
}

ID3D11DeviceContext4* CommandRecordingContext::GetD3D11DeviceContext4() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3d11DeviceContext4.Get();
}

ID3DUserDefinedAnnotation* CommandRecordingContext::GetD3DUserDefinedAnnotation() const {
    return mD3dUserDefinedAnnotation.Get();
}

Buffer* CommandRecordingContext::GetIndirectUniformBuffer() const {
    return mIndirectUniformBuffer.Get();
}

Device* CommandRecordingContext::GetDevice() const {
    DAWN_ASSERT(mDevice.Get());
    return mDevice.Get();
}

void CommandRecordingContext::Release() {
    if (mIsOpen) {
        DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
        mIsOpen = false;
        mNeedsSubmit = false;
        mIndirectUniformBuffer = nullptr;
        mD3d11UniformBuffer = nullptr;
        mDevice = nullptr;
        ID3D11Buffer* nullBuffer = nullptr;
        mD3d11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3d11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3d11DeviceContext4 = nullptr;
        mD3d11Device = nullptr;
    }
}

bool CommandRecordingContext::IsOpen() const {
    return mIsOpen;
}

bool CommandRecordingContext::NeedsSubmit() const {
    return mNeedsSubmit;
}

void CommandRecordingContext::SetNeedsSubmit() {
    mNeedsSubmit = true;
}

CommandRecordingContext::ScopedCriticalSection::ScopedCriticalSection(
    ComPtr<ID3D11Multithread> d3d11Multithread)
    : mD3d11Multithread(std::move(d3d11Multithread)) {
    if (mD3d11Multithread) {
        mD3d11Multithread->Enter();
    }
}

CommandRecordingContext::ScopedCriticalSection::~ScopedCriticalSection() {
    if (mD3d11Multithread) {
        mD3d11Multithread->Leave();
    }
}

CommandRecordingContext::ScopedCriticalSection
CommandRecordingContext::EnterScopedCriticalSection() {
    return ScopedCriticalSection(mD3d11Multithread);
}

void CommandRecordingContext::WriteUniformBuffer(uint32_t offset, uint32_t element) {
    DAWN_ASSERT(offset < kMaxNumBuiltinElements);
    if (mUniformBufferData[offset] != element) {
        mUniformBufferData[offset] = element;
        mUniformBufferDirty = true;
    }
}

MaybeError CommandRecordingContext::FlushUniformBuffer() {
    if (mUniformBufferDirty) {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        DAWN_TRY(
            CheckHRESULT(GetD3D11DeviceContext()->Map(mD3d11UniformBuffer.Get(),
                                                      /*Subresource=*/0, D3D11_MAP_WRITE_DISCARD,
                                                      /*MapFlags=*/0, &mappedResource),
                         "ID3D11DeviceContext::Map"));
        std::memcpy(mappedResource.pData, mUniformBufferData.data(),
                    mUniformBufferData.size() * sizeof(uint32_t));
        GetD3D11DeviceContext()->Unmap(mD3d11UniformBuffer.Get(), /*Subresource=*/0);
        mUniformBufferDirty = false;
    }
    return {};
}

void CommandRecordingContext::OnDrawOrDispatch(bool indirect) {
    if (mIsLastDrawOrDisplayIndirect != indirect) {
        mIsLastDrawOrDisplayIndirect = indirect;
        // Bind the indirect uniform buffer for indirect draw/dispatch.
        ID3D11Buffer* bufferPtr =
            indirect ? mIndirectUniformBuffer->GetD3D11ConstantBuffer() : mD3d11UniformBuffer.Get();
        mD3d11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &bufferPtr);
        mD3d11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &bufferPtr);
    }
}

}  // namespace dawn::native::d3d11
