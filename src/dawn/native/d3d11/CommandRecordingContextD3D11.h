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

#ifndef SRC_DAWN_NATIVE_D3D11_COMMANDRECORDINGCONTEXT_D3D11_H_
#define SRC_DAWN_NATIVE_D3D11_COMMANDRECORDINGCONTEXT_D3D11_H_

#include "dawn/common/NonCopyable.h"
#include "dawn/common/Ref.h"
#include "dawn/native/Error.h"
#include "dawn/native/d3d/d3d_platform.h"

namespace dawn::native::d3d11 {
class CommandAllocatorManager;
class Buffer;
class Device;
class CommandRecordingContext;

class DeviceContextTrackerD3D11 {
  public:
    void Initialize(ID3D11DeviceContext4* context4, CommandRecordingContext* commandContext);
    // Set viewport
    void RSSetViewports(const D3D11_VIEWPORT* pViewport);
    void RSSetScissorRects(const D3D11_RECT* pRect);

    void IASetVertexBuffers(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppVertexBuffers,
                            const UINT* pStrides,
                            const UINT* pOffsets);

    void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
    void IASetInputLayout(ID3D11InputLayout* pInputLayout);
    void RSSetState(ID3D11RasterizerState* pRasterizerState);
    void VSSetShader(ID3D11VertexShader* pVertexShader,
                     ID3D11ClassInstance* const* ppClassInstances,
                     UINT NumClassInstances);
    void PSSetShader(ID3D11PixelShader* pPixelShader,
                     ID3D11ClassInstance* const* ppClassInstances,
                     UINT NumClassInstances);
    void OMSetBlendState(ID3D11BlendState* pBlendState,
                         const FLOAT BlendFactor[4],
                         UINT SampleMask);
    void OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef);

  private:
    ID3D11DeviceContext4* mContext4;
    CommandRecordingContext* mCommandContext;

    D3D11_VIEWPORT mViewport = {};
    D3D11_RECT mScissorRect = {};

    ID3D11Buffer* mVertextBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT mVertexBufferStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT mVertexBufferOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};

    D3D11_PRIMITIVE_TOPOLOGY mTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11InputLayout* mInputLayout = nullptr;
    ID3D11RasterizerState* mRasterizerState = nullptr;
    ID3D11VertexShader* mVertexShader = nullptr;
    ID3D11PixelShader* mPixelShader = nullptr;
    ID3D11BlendState* mBlendState = nullptr;
    FLOAT mBlendFactor[4] = {};
    UINT mSampleMask = {};
    ID3D11DepthStencilState* mDepthStencilState = nullptr;
    UINT mStencilRef = {};
};

class CommandRecordingContext {
  public:
    MaybeError Intialize(Device* device);

    void Release();
    bool IsOpen() const;
    bool NeedsSubmit() const;
    void SetNeedsSubmit();

    MaybeError ExecuteCommandList(Device* device);

    ID3D11Device* GetD3D11Device() const;
    ID3D11DeviceContext* GetD3D11DeviceContext() const;
    ID3D11DeviceContext1* GetD3D11DeviceContext1() const;
    ID3D11DeviceContext4* GetD3D11DeviceContext4() const;
    ID3DUserDefinedAnnotation* GetD3DUserDefinedAnnotation() const;
    DeviceContextTrackerD3D11* GetDeviceContextTrackerD3D11();
    Buffer* GetUniformBuffer() const;
    Device* GetDevice() const;

    struct ScopedCriticalSection : NonMovable {
        explicit ScopedCriticalSection(ComPtr<ID3D11Multithread>);
        ~ScopedCriticalSection();

      private:
        ComPtr<ID3D11Multithread> mD3D11Multithread;
    };
    // Returns a scoped object that marks a critical section using the
    // ID3D11Multithread Enter and Leave methods. This allows minimizing the
    // cost of D3D11 multithread protection by allowing a single mutex Acquire
    // and Release call for an entire set of operations on the immediate context
    // e.g. when executing command buffers. This only has an effect if the
    // ImplicitDeviceSynchronization feature is enabled.
    ScopedCriticalSection EnterScopedCriticalSection();

    // Write the built-in variable value to the uniform buffer.
    void WriteUniformBuffer(uint32_t offset, uint32_t element);
    MaybeError FlushUniformBuffer();

    void Log(const char* message);

  private:
    bool mIsOpen = false;
    bool mNeedsSubmit = false;
    ComPtr<ID3D11Device> mD3D11Device;
    ComPtr<ID3D11DeviceContext4> mD3D11DeviceContext4;
    ComPtr<ID3D11Multithread> mD3D11Multithread;
    ComPtr<ID3DUserDefinedAnnotation> mD3DUserDefinedAnnotation;

    DeviceContextTrackerD3D11 mDeviceContextTrackerD3D11;

    // The maximum number of builtin elements is 4 (vec4). It must be multiple of 4.
    static constexpr size_t kMaxNumBuiltinElements = 4;
    // The uniform buffer for built-in variables.
    Ref<Buffer> mUniformBuffer;
    std::array<uint32_t, kMaxNumBuiltinElements> mUniformBufferData;
    bool mUniformBufferDirty = true;

    Ref<Device> mDevice;
};

}  // namespace dawn::native::d3d11

#endif  // SRC_DAWN_NATIVE_D3D11_COMMANDRECORDINGCONTEXT_D3D11_H_
