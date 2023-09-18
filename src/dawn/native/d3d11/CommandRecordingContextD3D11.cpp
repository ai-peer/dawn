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

void DeviceContextTrackerD3D11::Initialize(ID3D11DeviceContext4* context4,
                                           CommandRecordingContext* commandContext) {
    mContext4 = context4;
    mCommandContext = commandContext;
}

void DeviceContextTrackerD3D11::RSSetViewports(const D3D11_VIEWPORT* pViewport) {
    if (pViewport && *pViewport != mViewport) {
        mViewport = *pViewport;
        mCommandContext->Log("RSSetViewports");
        mContext4->RSSetViewports(1, pViewport);
    }
}

void DeviceContextTrackerD3D11::RSSetScissorRects(const D3D11_RECT* pRect) {
    if (pRect && *pRect != mScissorRect) {
        mScissorRect = *pRect;
        mCommandContext->Log("RSSetScissorRects");
        mContext4->RSSetScissorRects(1, pRect);
    }
}

void DeviceContextTrackerD3D11::IASetVertexBuffers(UINT StartSlot,
                                                   UINT NumBuffers,
                                                   ID3D11Buffer* const* ppVertexBuffers,
                                                   const UINT* pStrides,
                                                   const UINT* pOffsets) {
    bool areSame = true;
    for (UINT i = 0; i < NumBuffers; ++i) {
        UINT slot = i + StartSlot;
        if (ppVertexBuffers[i] != mVertextBuffers[slot] ||
            pStrides[i] != mVertexBufferStrides[slot] ||
            pOffsets[i] != mVertexBufferOffsets[slot]) {
            mVertextBuffers[slot] = ppVertexBuffers[i];
            mVertexBufferStrides[slot] = pStrides[i];
            mVertexBufferOffsets[slot] = pOffsets[i];
            areSame = false;
            break;
        }
    }
    if (!areSame) {
        mCommandContext->Log("IASetVertexBuffers");
        mContext4->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }
}

void DeviceContextTrackerD3D11::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    if (Topology != mTopology) {
        mTopology = Topology;
        mCommandContext->Log("IASetPrimitiveTopology");
        mContext4->IASetPrimitiveTopology(Topology);
    }
}

void DeviceContextTrackerD3D11::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
    if (pInputLayout != mInputLayout) {
        mInputLayout = pInputLayout;
        mCommandContext->Log("IASetInputLayout");
        mContext4->IASetInputLayout(pInputLayout);
    }
}

void DeviceContextTrackerD3D11::RSSetState(ID3D11RasterizerState* pRasterizerState) {
    if (pRasterizerState != mRasterizerState) {
        mRasterizerState = pRasterizerState;
        mCommandContext->Log("RSSetState");
        mContext4->RSSetState(pRasterizerState);
    }
}

void DeviceContextTrackerD3D11::VSSetShader(ID3D11VertexShader* pVertexShader,
                                            ID3D11ClassInstance* const* ppClassInstances,
                                            UINT NumClassInstances) {
    if (pVertexShader != mVertexShader) {
        mVertexShader = pVertexShader;
        mCommandContext->Log("VSSetShader");
        mContext4->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
    }
}

void DeviceContextTrackerD3D11::PSSetShader(ID3D11PixelShader* pPixelShader,
                                            ID3D11ClassInstance* const* ppClassInstances,
                                            UINT NumClassInstances) {
    if (pPixelShader != mPixelShader) {
        mPixelShader = pPixelShader;
        mCommandContext->Log("PSSetShader");
        mContext4->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
    }
}

void DeviceContextTrackerD3D11::OMSetBlendState(ID3D11BlendState* pBlendState,
                                                const FLOAT BlendFactor[4],
                                                UINT SampleMask) {
    if (pBlendState != mBlendState || BlendFactor[0] != mBlendFactor[0] ||
        BlendFactor[1] != mBlendFactor[1] || BlendFactor[2] != mBlendFactor[2] ||
        BlendFactor[3] != mBlendFactor[3] || SampleMask != mSampleMask) {
        mBlendState = pBlendState;
        std::memcpy(mBlendFactor, BlendFactor, sizeof(mBlendFactor));
        mSampleMask = SampleMask;
        mCommandContext->Log("OMSetBlendState");
        mContext4->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
    }
}

void DeviceContextTrackerD3D11::OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState,
                                                       UINT StencilRef) {
    if (pDepthStencilState != mDepthStencilState || StencilRef != mStencilRef) {
        mDepthStencilState = pDepthStencilState;
        mStencilRef = StencilRef;
        mCommandContext->Log("OMSetDepthStencilState");
        mContext4->OMSetDepthStencilState(pDepthStencilState, StencilRef);
    }
}

void CommandRecordingContext::Log(const char* message) {
    // GetDevice()->EmitLog(WGPULoggingType_Error, message);
}

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
        CheckHRESULT(d3d11DeviceContext4.As(&mD3DUserDefinedAnnotation),
                     "D3D11 querying immediate context for ID3DUserDefinedAnnotation interface"));

    if (device->HasFeature(Feature::D3D11MultithreadProtected)) {
        DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&mD3D11Multithread),
                              "D3D11 querying immediate context for ID3D11Multithread interface"));
        mD3D11Multithread->SetMultithreadProtected(TRUE);
    }

    mD3D11Device = d3d11Device;
    mD3D11DeviceContext4 = std::move(d3d11DeviceContext4);

    mDeviceContextTrackerD3D11.Initialize(mD3D11DeviceContext4.Get(), this);

    mIsOpen = true;

    // Create a uniform buffer for built in variables.
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
    mUniformBuffer = ToBackend(std::move(uniformBuffer));

    // Always bind the uniform buffer to the reserved slot for all pipelines.
    // This buffer will be updated with the correct values before each draw or dispatch call.
    ID3D11Buffer* bufferPtr = mUniformBuffer->GetD3D11ConstantBuffer();
    mD3D11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                               &bufferPtr);
    mD3D11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                               &bufferPtr);

    return {};
}

MaybeError CommandRecordingContext::ExecuteCommandList(Device* device) {
    // Consider using deferred DeviceContext.
    mNeedsSubmit = false;
    return {};
}

ID3D11Device* CommandRecordingContext::GetD3D11Device() const {
    return mD3D11Device.Get();
}

ID3D11DeviceContext* CommandRecordingContext::GetD3D11DeviceContext() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3D11DeviceContext4.Get();
}

ID3D11DeviceContext1* CommandRecordingContext::GetD3D11DeviceContext1() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3D11DeviceContext4.Get();
}

ID3D11DeviceContext4* CommandRecordingContext::GetD3D11DeviceContext4() const {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return mD3D11DeviceContext4.Get();
}

DeviceContextTrackerD3D11* CommandRecordingContext::GetDeviceContextTrackerD3D11() {
    DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
    return &mDeviceContextTrackerD3D11;
}

ID3DUserDefinedAnnotation* CommandRecordingContext::GetD3DUserDefinedAnnotation() const {
    return mD3DUserDefinedAnnotation.Get();
}

Buffer* CommandRecordingContext::GetUniformBuffer() const {
    return mUniformBuffer.Get();
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
        mUniformBuffer = nullptr;
        mDevice = nullptr;
        ID3D11Buffer* nullBuffer = nullptr;
        mD3D11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3D11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3D11DeviceContext4 = nullptr;
        mD3D11Device = nullptr;
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
    : mD3D11Multithread(std::move(d3d11Multithread)) {
    if (mD3D11Multithread) {
        mD3D11Multithread->Enter();
    }
}

CommandRecordingContext::ScopedCriticalSection::~ScopedCriticalSection() {
    if (mD3D11Multithread) {
        mD3D11Multithread->Leave();
    }
}

CommandRecordingContext::ScopedCriticalSection
CommandRecordingContext::EnterScopedCriticalSection() {
    return ScopedCriticalSection(mD3D11Multithread);
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
        DAWN_TRY(mUniformBuffer->Write(this, 0, mUniformBufferData.data(),
                                       mUniformBufferData.size() * sizeof(uint32_t)));
        mUniformBufferDirty = false;
    }
    return {};
}

}  // namespace dawn::native::d3d11
