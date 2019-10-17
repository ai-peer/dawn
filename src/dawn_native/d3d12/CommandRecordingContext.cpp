// Copyright 2019 The Dawn Authors
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
#include "dawn_native/d3d12/CommandRecordingContext.h"
#include "dawn_native/d3d12/CommandAllocatorManager.h"
#include "dawn_native/d3d12/D3D12Error.h"

namespace dawn_native { namespace d3d12 {

    void CommandRecordingContext::AddToSharedTextureList(Texture* texture) {
        ASSERT(IsOpen());
        mSharedTextures.insert(texture);
    }

    MaybeError CommandRecordingContext::Open(ID3D12Device* d3d12Device,
                                             CommandAllocatorManager* commandAllocationManager) {
        ASSERT(!IsOpen());
        ID3D12CommandAllocator* commandAllocator;
        DAWN_TRY_ASSIGN(commandAllocator, commandAllocationManager->ReserveCommandAllocator());
        if (mD3d12CommandList != nullptr) {
            MaybeError error = CheckHRESULT(mD3d12CommandList->Reset(commandAllocator, nullptr),
                                            "D3D12 resetting command list");
            if (error.IsError()) {
                mD3d12CommandList.Reset();
                DAWN_TRY(std::move(error));
            }
        } else {
            ComPtr<ID3D12GraphicsCommandList> d3d12GraphicsCommandList;
            DAWN_TRY(CheckHRESULT(
                d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator,
                                               nullptr, IID_PPV_ARGS(&d3d12GraphicsCommandList)),
                "D3D12 creating direct command list"));
            mD3d12CommandList = std::move(d3d12GraphicsCommandList);
        }

        mIsOpen = true;

        return {};
    }

    MaybeError CommandRecordingContext::ExecuteCommandList(ID3D12CommandQueue* d3d12CommandQueue) {
        if (IsOpen()) {
            for (Texture* texture : mSharedTextures) {
                texture->TransitionUsageNow(this, D3D12_RESOURCE_STATE_COMMON);
            }

            MaybeError error =
                CheckHRESULT(mD3d12CommandList->Close(), "D3D12 closing pending command list");
            if (error.IsError()) {
                Release();
                DAWN_TRY(std::move(error));
            }

            ID3D12CommandList* d3d12CommandList = GetCommandList();
            d3d12CommandQueue->ExecuteCommandLists(1, &d3d12CommandList);

            mIsOpen = false;
            mSharedTextures.clear();
        }
        return {};
    }

    ID3D12GraphicsCommandList* CommandRecordingContext::GetCommandList() const {
        ASSERT(mD3d12CommandList != nullptr);
        ASSERT(IsOpen());
        return mD3d12CommandList.Get();
    }

    void CommandRecordingContext::Release() {
        mD3d12CommandList.Reset();
        mIsOpen = false;
        mSharedTextures.clear();
    }

    bool CommandRecordingContext::IsOpen() const {
        return mIsOpen;
    }

}}  // namespace dawn_native::d3d12
