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

#include "dawn_native/opengl/BlendStateGL.h"
#include "dawn_native/opengl/DeviceGL.h"

#include "common/Assert.h"

namespace dawn_native { namespace opengl {

    namespace {
        GLenum GLBlendFactor(dawn::BlendFactor factor, bool alpha) {
            switch (factor) {
                case dawn::BlendFactor::Zero:
                    return GL_ZERO;
                case dawn::BlendFactor::One:
                    return GL_ONE;
                case dawn::BlendFactor::SrcColor:
                    return GL_SRC_COLOR;
                case dawn::BlendFactor::OneMinusSrcColor:
                    return GL_ONE_MINUS_SRC_COLOR;
                case dawn::BlendFactor::SrcAlpha:
                    return GL_SRC_ALPHA;
                case dawn::BlendFactor::OneMinusSrcAlpha:
                    return GL_ONE_MINUS_SRC_ALPHA;
                case dawn::BlendFactor::DstColor:
                    return GL_DST_COLOR;
                case dawn::BlendFactor::OneMinusDstColor:
                    return GL_ONE_MINUS_DST_COLOR;
                case dawn::BlendFactor::DstAlpha:
                    return GL_DST_ALPHA;
                case dawn::BlendFactor::OneMinusDstAlpha:
                    return GL_ONE_MINUS_DST_ALPHA;
                case dawn::BlendFactor::SrcAlphaSaturated:
                    return GL_SRC_ALPHA_SATURATE;
                case dawn::BlendFactor::BlendColor:
                    return alpha ? GL_CONSTANT_ALPHA : GL_CONSTANT_COLOR;
                case dawn::BlendFactor::OneMinusBlendColor:
                    return alpha ? GL_ONE_MINUS_CONSTANT_ALPHA : GL_ONE_MINUS_CONSTANT_COLOR;
                default:
                    UNREACHABLE();
            }
        }

        GLenum GLBlendMode(dawn::BlendOperation operation) {
            switch (operation) {
                case dawn::BlendOperation::Add:
                    return GL_FUNC_ADD;
                case dawn::BlendOperation::Subtract:
                    return GL_FUNC_SUBTRACT;
                case dawn::BlendOperation::ReverseSubtract:
                    return GL_FUNC_REVERSE_SUBTRACT;
                case dawn::BlendOperation::Min:
                    return GL_MIN;
                case dawn::BlendOperation::Max:
                    return GL_MAX;
                default:
                    UNREACHABLE();
            }
        }
    }  // namespace

    BlendState::BlendState(Device* device, const BlendStateDescriptor* descriptor)
        : BlendStateBase(device, descriptor) {
    }

    void BlendState::ApplyNow(uint32_t attachment) {
        const BlendStateDescriptor* descriptor = GetBlendStateDescriptor();

        if (descriptor->blendEnabled) {
            glEnablei(GL_BLEND, attachment);
            glBlendEquationSeparatei(attachment, GLBlendMode(descriptor->colorBlend.operation),
                                     GLBlendMode(descriptor->alphaBlend.operation));
            glBlendFuncSeparatei(attachment, GLBlendFactor(descriptor->colorBlend.srcFactor, false),
                                 GLBlendFactor(descriptor->colorBlend.dstFactor, false),
                                 GLBlendFactor(descriptor->alphaBlend.srcFactor, true),
                                 GLBlendFactor(descriptor->alphaBlend.dstFactor, true));
        } else {
            glDisablei(GL_BLEND, attachment);
        }
        glColorMaski(attachment, descriptor->colorWriteMask & dawn::ColorWriteMask::Red,
                     descriptor->colorWriteMask & dawn::ColorWriteMask::Green,
                     descriptor->colorWriteMask & dawn::ColorWriteMask::Blue,
                     descriptor->colorWriteMask & dawn::ColorWriteMask::Alpha);
    }

}}  // namespace dawn_native::opengl
