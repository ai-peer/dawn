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

#include "dawn_native/metal/InputStateMTL.h"

#include "common/BitSetIterator.h"

namespace dawn_native { namespace metal {

    namespace {
        MTLVertexFormat VertexFormatType(dawn::VertexFormat format) {
            switch (format) {
                case dawn::VertexFormat::FloatR32G32B32A32:
                    return MTLVertexFormatFloat4;
                case dawn::VertexFormat::FloatR32G32B32:
                    return MTLVertexFormatFloat3;
                case dawn::VertexFormat::FloatR32G32:
                    return MTLVertexFormatFloat2;
                case dawn::VertexFormat::FloatR32:
                    return MTLVertexFormatFloat;
                case dawn::VertexFormat::IntR32G32B32A32:
                    return MTLVertexFormatInt4;
                case dawn::VertexFormat::IntR32G32B32:
                    return MTLVertexFormatInt3;
                case dawn::VertexFormat::IntR32G32:
                    return MTLVertexFormatInt2;
                case dawn::VertexFormat::IntR32:
                    return MTLVertexFormatInt;
                case dawn::VertexFormat::UshortR16G16B16A16:
                    return MTLVertexFormatUShort4;
                case dawn::VertexFormat::UshortR16G16:
                    return MTLVertexFormatUShort2;
                case dawn::VertexFormat::UnormR8G8B8A8:
                    return MTLVertexFormatUChar4Normalized;
                case dawn::VertexFormat::UnormR8G8:
                    return MTLVertexFormatUChar2Normalized;
            }
        }

        MTLVertexStepFunction InputStepModeFunction(dawn::InputStepMode mode) {
            switch (mode) {
                case dawn::InputStepMode::Vertex:
                    return MTLVertexStepFunctionPerVertex;
                case dawn::InputStepMode::Instance:
                    return MTLVertexStepFunctionPerInstance;
            }
        }
    }

    InputState::InputState(InputStateBuilder* builder) : InputStateBase(builder) {
        mMtlVertexDescriptor = [MTLVertexDescriptor new];

        const auto& attributesSetMask = GetAttributesSetMask();
        for (uint32_t i = 0; i < attributesSetMask.size(); ++i) {
            if (!attributesSetMask[i]) {
                continue;
            }
            const VertexAttributeDescriptor& info = GetAttribute(i);

            auto attribDesc = [MTLVertexAttributeDescriptor new];
            attribDesc.format = VertexFormatType(info.format);
            attribDesc.offset = info.offset;
            attribDesc.bufferIndex = kMaxBindingsPerGroup + info.inputSlot;
            mMtlVertexDescriptor.attributes[i] = attribDesc;
            [attribDesc release];
        }
        // NSMutableArray* strideArr = nullptr;
        uint32_t max_stride = 0;
        for (uint32_t i : IterateBitSet(GetInputsSetMask())) {
            const VertexInputDescriptor& info = GetInput(i);

            auto layoutDesc = [MTLVertexBufferLayoutDescriptor new];
            if (info.stride == 0) {
                // For MTLVertexStepFunctionConstant, the stepRate must be 0,
                // but the stride must NOT be 0, so we made up it to be
                // max(attrib.offset + sizeof(attrib) for each attrib)
                // strideArr = [NSMutableArray array];
                for (uint32_t n = 0; n < attributesSetMask.size(); ++n) {
                    if (!attributesSetMask[n]) {
                        continue;
                    }
                    const VertexAttributeDescriptor& attribute = GetAttribute(n);
                    // NSNumber* number = [NSNumber
                    //     numberWithInt:VertexFormatSize(attribute.format) + attribute.offset];
                    // [strideArr addObject:number];
                    uint32_t size = VertexFormatSize(attribute.format) + attribute.offset;
                    max_stride = max_stride > size ? max_stride : size;
                }
                layoutDesc.stepFunction = MTLVertexStepFunctionConstant;
                layoutDesc.stepRate = 0;
                layoutDesc.stride = max_stride;
                // layoutDesc.stride = [[strideArr valueForKeyPath:@"@max.intValue"] intValue];
            } else {
                layoutDesc.stepFunction = InputStepModeFunction(info.stepMode);
                layoutDesc.stepRate = 1;
                layoutDesc.stride = info.stride;
            }
            // TODO(cwallez@chromium.org): make the offset depend on the pipeline layout
            mMtlVertexDescriptor.layouts[kMaxBindingsPerGroup + i] = layoutDesc;
            [layoutDesc release];
        }
        // if (strideArr != nullptr) {
        //     [strideArr release];
        // }
    }

    InputState::~InputState() {
        [mMtlVertexDescriptor release];
        mMtlVertexDescriptor = nil;
    }

    MTLVertexDescriptor* InputState::GetMTLVertexDescriptor() {
        return mMtlVertexDescriptor;
    }

}}  // namespace dawn_native::metal
