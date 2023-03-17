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

#include "dawn/native/d3d/PipelineLayoutD3D.h"

#include "dawn/common/BitSetIterator.h"
#include "dawn/native/d3d/BindGroupLayoutD3D.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/DeviceD3D.h"

namespace dawn::native::d3d {

PipelineLayout::PipelineLayout(Device* device, const PipelineLayoutDescriptor* descriptor)
    : PipelineLayoutBase(device, descriptor) {}

PipelineLayout::~PipelineLayout() = default;

MaybeError PipelineLayout::Initialize() {
    // Loops over all of the dynamic storage buffer bindings in the layout and build
    // a mapping from the binding to the next offset into the root constant array where
    // that dynamic storage buffer's binding size will be stored. The next register offset
    // to use is tracked with |dynamicStorageBufferLengthsShaderRegisterOffset|.
    // This data will be used by shader translation to emit a load from the root constant
    // array to use as the binding's size in runtime array calculations.
    // Each bind group's length data is stored contiguously in the root constant array,
    // so the loop also computes the first register offset for each group where the
    // data should start.
    uint32_t dynamicStorageBufferLengthsShaderRegisterOffset = 0;
    for (BindGroupIndex group : IterateBitSet(GetBindGroupLayoutsMask())) {
        const BindGroupLayoutBase* bgl = GetBindGroupLayout(group);

        mDynamicStorageBufferLengthInfo[group].firstRegisterOffset =
            dynamicStorageBufferLengthsShaderRegisterOffset;
        mDynamicStorageBufferLengthInfo[group].bindingAndRegisterOffsets.reserve(
            bgl->GetBindingCountInfo().dynamicStorageBufferCount);

        for (BindingIndex bindingIndex(0); bindingIndex < bgl->GetDynamicBufferCount();
             ++bindingIndex) {
            if (bgl->IsStorageBufferBinding(bindingIndex)) {
                mDynamicStorageBufferLengthInfo[group].bindingAndRegisterOffsets.push_back(
                    {bgl->GetBindingInfo(bindingIndex).binding,
                     dynamicStorageBufferLengthsShaderRegisterOffset++});
            }
        }

        ASSERT(mDynamicStorageBufferLengthInfo[group].bindingAndRegisterOffsets.size() ==
               bgl->GetBindingCountInfo().dynamicStorageBufferCount);
    }

    mDynamicStorageBufferLengthsShaderRegisterOffset =
        dynamicStorageBufferLengthsShaderRegisterOffset;

    return {};
}

const PipelineLayout::DynamicStorageBufferLengthInfo&
PipelineLayout::GetDynamicStorageBufferLengthInfo() const {
    return mDynamicStorageBufferLengthInfo;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetFirstIndexOffsetRegisterSpace() const {
    return kRenderOrComputeInternalRegisterSpace;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetFirstIndexOffsetShaderRegister() const {
    return kRenderOrComputeInternalBaseRegister;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetNumWorkgroupsRegisterSpace() const {
    return kRenderOrComputeInternalRegisterSpace;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetNumWorkgroupsShaderRegister() const {
    return kRenderOrComputeInternalBaseRegister;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetDynamicStorageBufferLengthsRegisterSpace() const {
    return kDynamicStorageBufferLengthsRegisterSpace;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetDynamicStorageBufferLengthsShaderRegister() const {
    return kDynamicStorageBufferLengthsBaseRegister;
}

// TODO(crbug.com/dawn/1716): figure how to setup space & register for D3D11
uint32_t PipelineLayout::GetDynamicStorageBufferLengthsShaderRegisterOffset() const {
    return mDynamicStorageBufferLengthsShaderRegisterOffset;
}

}  // namespace dawn::native::d3d
