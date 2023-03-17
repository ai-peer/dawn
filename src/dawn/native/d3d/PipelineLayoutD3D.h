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

#ifndef SRC_DAWN_NATIVE_D3D_PIPELINELAYOUTD3D_H_
#define SRC_DAWN_NATIVE_D3D_PIPELINELAYOUTD3D_H_

#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/common/ityp_array.h"
#include "dawn/common/ityp_vector.h"
#include "dawn/native/PipelineLayout.h"

namespace dawn::native::d3d {

class Device;

class PipelineLayout : public PipelineLayoutBase {
  public:
    PipelineLayout(Device* device, const PipelineLayoutDescriptor* descriptor);
    ~PipelineLayout() override;

    MaybeError Initialize();

    virtual uint32_t GetFirstIndexOffsetRegisterSpace() const = 0;
    virtual uint32_t GetFirstIndexOffsetShaderRegister() const = 0;

    virtual uint32_t GetNumWorkgroupsRegisterSpace() const = 0;
    virtual uint32_t GetNumWorkgroupsShaderRegister() const = 0;

    virtual uint32_t GetDynamicStorageBufferLengthsRegisterSpace() const = 0;
    virtual uint32_t GetDynamicStorageBufferLengthsShaderRegister() const = 0;
    uint32_t GetDynamicStorageBufferLengthsShaderRegisterOffset() const;

    struct PerBindGroupDynamicStorageBufferLengthInfo {
        // First register offset for a bind group's dynamic storage buffer lengths.
        // This is the index into the array of root constants where this bind group's
        // lengths start.
        uint32_t firstRegisterOffset;

        struct BindingAndRegisterOffset {
            BindingNumber binding;
            uint32_t registerOffset;
        };
        // Associative list of (BindingNumber,registerOffset) pairs, which is passed into
        // the shader to map the BindingPoint(thisGroup, BindingNumber) to the registerOffset
        // into the root constant array which holds the dynamic storage buffer lengths.
        std::vector<BindingAndRegisterOffset> bindingAndRegisterOffsets;
    };

    // Flat map from bind group index to the list of (BindingNumber,Register) pairs.
    // Each pair is used in shader translation to
    using DynamicStorageBufferLengthInfo =
        ityp::array<BindGroupIndex, PerBindGroupDynamicStorageBufferLengthInfo, kMaxBindGroups>;

    const DynamicStorageBufferLengthInfo& GetDynamicStorageBufferLengthInfo() const;

  private:
    DynamicStorageBufferLengthInfo mDynamicStorageBufferLengthInfo;
    uint32_t mDynamicStorageBufferLengthsShaderRegisterOffset = 0;
};

}  // namespace dawn::native::d3d

#endif  // SRC_DAWN_NATIVE_D3D_PIPELINELAYOUTD3D_H_
