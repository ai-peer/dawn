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

#include "dawn_native/opengl/SamplerGL.h"

#include "common/Assert.h"
#include "dawn_native/opengl/DeviceGL.h"

namespace dawn_native { namespace opengl {

    namespace {
        GLenum MagFilterMode(dawn::FilterMode filter) {
            switch (filter) {
                case dawn::FilterMode::Nearest:
                    return GL_NEAREST;
                case dawn::FilterMode::Linear:
                    return GL_LINEAR;
                default:
                    UNREACHABLE();
            }
        }

        GLenum MinFilterMode(dawn::FilterMode minFilter, dawn::FilterMode mipMapFilter) {
            switch (minFilter) {
                case dawn::FilterMode::Nearest:
                    switch (mipMapFilter) {
                        case dawn::FilterMode::Nearest:
                            return GL_NEAREST_MIPMAP_NEAREST;
                        case dawn::FilterMode::Linear:
                            return GL_NEAREST_MIPMAP_LINEAR;
                        default:
                            UNREACHABLE();
                    }
                case dawn::FilterMode::Linear:
                    switch (mipMapFilter) {
                        case dawn::FilterMode::Nearest:
                            return GL_LINEAR_MIPMAP_NEAREST;
                        case dawn::FilterMode::Linear:
                            return GL_LINEAR_MIPMAP_LINEAR;
                        default:
                            UNREACHABLE();
                    }
                default:
                    UNREACHABLE();
            }
        }

        GLenum WrapMode(dawn::AddressMode mode) {
            switch (mode) {
                case dawn::AddressMode::Repeat:
                    return GL_REPEAT;
                case dawn::AddressMode::MirroredRepeat:
                    return GL_MIRRORED_REPEAT;
                case dawn::AddressMode::ClampToEdge:
                    return GL_CLAMP_TO_EDGE;
                case dawn::AddressMode::ClampToBorderColor:
                    return GL_CLAMP_TO_BORDER;
                default:
                    UNREACHABLE();
            }
        }

        GLenum CompareFunction(dawn::CompareFunction compareOp) {
            switch (compareOp) {
                case dawn::CompareFunction::Never:
                    return GL_NEVER;
                case dawn::CompareFunction::Less:
                    return GL_LESS;
                case dawn::CompareFunction::LessEqual:
                    return GL_LEQUAL;
                case dawn::CompareFunction::Greater:
                    return GL_GREATER;
                case dawn::CompareFunction::GreaterEqual:
                    return GL_GEQUAL;
                case dawn::CompareFunction::Equal:
                    return GL_EQUAL;
                case dawn::CompareFunction::NotEqual:
                    return GL_NOTEQUAL;
                case dawn::CompareFunction::Always:
                    return GL_ALWAYS;
                default:
                    UNREACHABLE();
            }
        }

    }  // namespace

    Sampler::Sampler(Device* device, const SamplerDescriptor* descriptor)
        : SamplerBase(device, descriptor) {
        glGenSamplers(1, &mHandle);
        glSamplerParameteri(mHandle, GL_TEXTURE_MAG_FILTER, MagFilterMode(descriptor->magFilter));
        glSamplerParameteri(mHandle, GL_TEXTURE_MIN_FILTER,
                            MinFilterMode(descriptor->minFilter, descriptor->mipmapFilter));
        glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_R, WrapMode(descriptor->rAddressMode));
        glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_S, WrapMode(descriptor->sAdressMode));
        glSamplerParameteri(mHandle, GL_TEXTURE_WRAP_T, WrapMode(descriptor->tAddressMode));

        glSamplerParameterf(mHandle, GL_TEXTURE_MIN_LOD, descriptor->lodMinClamp);
        glSamplerParameterf(mHandle, GL_TEXTURE_MAX_LOD, descriptor->lodMaxClamp);

        if (CompareFunction(descriptor->compareFunction) != GL_NEVER) {
            glSamplerParameteri(mHandle, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(mHandle, GL_TEXTURE_COMPARE_FUNC,
                                CompareFunction(descriptor->compareFunction));
        }

        switch (descriptor->borderColor) {
            case dawn::BorderColor::TransparentBlack:
                glSamplerParameterfv(mHandle, GL_TEXTURE_BORDER_COLOR,
                                     new float[]{0.0, 0.0, 0.0, 0.0});
                break;
            case dawn::BorderColor::OpaqueBlack:
                glSamplerParameterfv(mHandle, GL_TEXTURE_BORDER_COLOR,
                                     new float[]{0.0, 0.0, 0.0, 1.0});
                break;
            case dawn::BorderColor::OpaqueWhite:
                glSamplerParameterfv(mHandle, GL_TEXTURE_BORDER_COLOR,
                                     new float[]{1.0, 1.0, 1.0, 1.0});
                break;
            default:
                UNREACHABLE();
        }
    }

    GLuint Sampler::GetHandle() const {
        return mHandle;
    }

}}  // namespace dawn_native::opengl
