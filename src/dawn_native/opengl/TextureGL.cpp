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

#include "dawn_native/opengl/TextureGL.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "dawn_native/opengl/BufferGL.h"
#include "dawn_native/opengl/DeviceGL.h"
#include "dawn_native/opengl/UtilsGL.h"

namespace dawn_native { namespace opengl {

    namespace {

        GLenum TargetForTexture(const TextureDescriptor* descriptor) {
            switch (descriptor->dimension) {
                case dawn::TextureDimension::e2D:
                    if (descriptor->arrayLayerCount > 1) {
                        ASSERT(descriptor->sampleCount == 1);
                        return GL_TEXTURE_2D_ARRAY;
                    } else {
                        if (descriptor->sampleCount > 1) {
                            return GL_TEXTURE_2D_MULTISAMPLE;
                        } else {
                            return GL_TEXTURE_2D;
                        }
                    }

                default:
                    UNREACHABLE();
                    return GL_TEXTURE_2D;
            }
        }

        GLenum TargetForTextureViewDimension(dawn::TextureViewDimension dimension,
                                             uint32_t sampleCount) {
            switch (dimension) {
                case dawn::TextureViewDimension::e2D:
                    return (sampleCount > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                case dawn::TextureViewDimension::e2DArray:
                    ASSERT(sampleCount == 1);
                    return GL_TEXTURE_2D_ARRAY;
                case dawn::TextureViewDimension::Cube:
                    return GL_TEXTURE_CUBE_MAP;
                case dawn::TextureViewDimension::CubeArray:
                    return GL_TEXTURE_CUBE_MAP_ARRAY;
                default:
                    UNREACHABLE();
                    return GL_TEXTURE_2D;
            }
        }

        GLuint GenTexture(const OpenGLFunctions& gl) {
            GLuint handle = 0;
            gl.GenTextures(1, &handle);
            return handle;
        }

        bool UsageNeedsTextureView(dawn::TextureUsage usage) {
            constexpr dawn::TextureUsage kUsageNeedingTextureView =
                dawn::TextureUsage::Storage | dawn::TextureUsage::Sampled;
            return usage & kUsageNeedingTextureView;
        }

        bool RequiresCreatingNewTextureView(const TextureBase* texture,
                                            const TextureViewDescriptor* textureViewDescriptor) {
            if (texture->GetFormat().format != textureViewDescriptor->format) {
                return true;
            }

            if (texture->GetArrayLayers() != textureViewDescriptor->arrayLayerCount) {
                return true;
            }

            if (texture->GetNumMipLevels() != textureViewDescriptor->mipLevelCount) {
                return true;
            }

            switch (textureViewDescriptor->dimension) {
                case dawn::TextureViewDimension::Cube:
                case dawn::TextureViewDimension::CubeArray:
                    return true;
                default:
                    break;
            }

            return false;
        }

    }  // namespace

    // Texture

    Texture::Texture(Device* device, const TextureDescriptor* descriptor)
        : Texture(device, descriptor, GenTexture(device->gl), TextureState::OwnedInternal) {
        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

        uint32_t width = GetSize().width;
        uint32_t height = GetSize().height;
        uint32_t levels = GetNumMipLevels();
        uint32_t arrayLayers = GetArrayLayers();
        uint32_t sampleCount = GetSampleCount();

        const GLFormat& glFormat = GetGLFormat();

        gl.BindTexture(mTarget, mHandle);

        // glTextureView() requires the value of GL_TEXTURE_IMMUTABLE_FORMAT for origtexture to be
        // GL_TRUE, so the storage of the texture must be allocated with glTexStorage*D.
        // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTextureView.xhtml
        switch (GetDimension()) {
            case dawn::TextureDimension::e2D:
                if (arrayLayers > 1) {
                    ASSERT(!IsMultisampledTexture());
                    gl.TexStorage3D(mTarget, levels, glFormat.internalFormat, width, height,
                                    arrayLayers);
                } else {
                    if (IsMultisampledTexture()) {
                        gl.TexStorage2DMultisample(mTarget, sampleCount, glFormat.internalFormat,
                                                   width, height, true);
                    } else {
                        gl.TexStorage2D(mTarget, levels, glFormat.internalFormat, width, height);
                    }
                }
                break;
            default:
                UNREACHABLE();
        }

        // The texture is not complete if it uses mipmapping and not all levels up to
        // MAX_LEVEL have been defined.
        gl.TexParameteri(mTarget, GL_TEXTURE_MAX_LEVEL, levels - 1);

        if (GetDevice()->IsToggleEnabled(Toggle::NonzeroClearResourcesOnCreationForTesting)) {
            GetDevice()->ConsumedError(ClearTexture(0, GetNumMipLevels(), 0, GetArrayLayers(),
                                                    TextureBase::ClearValue::NonZero));
        }
    }

    Texture::Texture(Device* device,
                     const TextureDescriptor* descriptor,
                     GLuint handle,
                     TextureState state)
        : TextureBase(device, descriptor, state), mHandle(handle) {
        mTarget = TargetForTexture(descriptor);
    }

    Texture::~Texture() {
        DestroyInternal();
    }

    void Texture::DestroyImpl() {
        ToBackend(GetDevice())->gl.DeleteTextures(1, &mHandle);
        mHandle = 0;
    }

    GLuint Texture::GetHandle() const {
        return mHandle;
    }

    GLenum Texture::GetGLTarget() const {
        return mTarget;
    }

    const GLFormat& Texture::GetGLFormat() const {
        return ToBackend(GetDevice())->GetGLFormat(GetFormat());
    }

    MaybeError Texture::ClearTexture(GLint baseMipLevel,
                                     GLint levelCount,
                                     GLint baseArrayLayer,
                                     uint32_t layerCount,
                                     TextureBase::ClearValue clearValue) {
        // TODO(jiawei.shao@intel.com): initialize the textures with compressed formats.
        if (GetFormat().isCompressed) {
            return {};
        }

        Device* device = ToBackend(GetDevice());
        const OpenGLFunctions& gl = device->gl;
        uint8_t clearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0 : 1;
        if (GetFormat().isRenderable) {
            if (GetFormat().HasDepthOrStencil()) {
                bool doDepthClear = GetFormat().HasDepth();
                bool doStencilClear = GetFormat().HasStencil();
                GLfloat depth = clearColor;
                GLint stencil = clearColor;
                if (doDepthClear) {
                    gl.DepthMask(GL_TRUE);
                }
                if (doStencilClear) {
                    gl.StencilMask(GetStencilMaskFromStencilFormat(GetFormat().format));
                }

                GLuint framebuffer = 0;
                gl.GenFramebuffers(1, &framebuffer);
                gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
                gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                        GetGLTarget(), GetHandle(), 0);
                if (doDepthClear && doStencilClear) {
                    gl.ClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
                } else if (doDepthClear) {
                    gl.ClearBufferfv(GL_DEPTH, 0, &depth);
                } else if (doStencilClear) {
                    gl.ClearBufferiv(GL_STENCIL, 0, &stencil);
                }
                gl.DeleteFramebuffers(1, &framebuffer);
            } else {
                static constexpr uint32_t MAX_TEXEL_SIZE = 16;
                ASSERT(GetFormat().blockByteSize <= MAX_TEXEL_SIZE);
                GLbyte clearColorData[MAX_TEXEL_SIZE];
                clearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0 : 255;
                std::fill(clearColorData, clearColorData + MAX_TEXEL_SIZE, clearColor);

                const GLFormat& glFormat = GetGLFormat();
                for (GLint level = baseMipLevel; level < baseMipLevel + levelCount; ++level) {
                    Extent3D mipSize = GetMipLevelPhysicalSize(level);
                    gl.ClearTexSubImage(mHandle, level, 0, 0, baseArrayLayer, mipSize.width,
                                        mipSize.height, layerCount, glFormat.format, glFormat.type,
                                        clearColorData);
                }
            }
        } else {
            // TODO(natlee@microsoft.com): test compressed textures are cleared
            // create temp buffer with clear color to copy to the texture image
            uint32_t rowPitch =
                Align((GetSize().width / GetFormat().blockWidth) * GetFormat().blockByteSize,
                      kTextureRowPitchAlignment);
            dawn_native::BufferDescriptor descriptor;
            descriptor.size = rowPitch * (GetSize().height / GetFormat().blockHeight);

            descriptor.nextInChain = nullptr;
            descriptor.usage = dawn::BufferUsage::CopySrc | dawn::BufferUsage::MapWrite;
            std::unique_ptr<Buffer> srcBuffer =
                std::make_unique<dawn_native::opengl::Buffer>(device, &descriptor);
            uint8_t* clearBuffer = nullptr;
            MaybeError error = srcBuffer->MapAtCreation(&clearBuffer);
            if (DAWN_UNLIKELY(error.IsError())) {
                return error;
            }

            std::fill(reinterpret_cast<uint32_t*>(clearBuffer),
                      reinterpret_cast<uint32_t*>(clearBuffer + descriptor.size), clearColor);
            srcBuffer->Unmap();

            for (GLint level = baseMipLevel; level < baseMipLevel + levelCount; ++level) {
                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, srcBuffer->GetHandle());
                gl.ActiveTexture(GL_TEXTURE0);
                gl.BindTexture(GetGLTarget(), GetHandle());

                gl.PixelStorei(GL_UNPACK_ROW_LENGTH,
                               rowPitch / GetFormat().blockByteSize * GetFormat().blockWidth);
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

                Extent3D size = GetMipLevelPhysicalSize(level);
                switch (GetDimension()) {
                    case dawn::TextureDimension::e2D:
                        if (layerCount > 1) {
                            gl.TexSubImage3D(GetGLTarget(), level, 0, 0, baseArrayLayer, size.width,
                                             size.height, 1, GetGLFormat().format,
                                             GetGLFormat().type,
                                             reinterpret_cast<void*>(static_cast<uintptr_t>(0)));
                        } else {
                            gl.TexSubImage2D(GetGLTarget(), level, 0, 0, size.width, size.height,
                                             GetGLFormat().format, GetGLFormat().type, 0);
                        }
                        break;

                    default:
                        UNREACHABLE();
                }
                gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
        return {};
    }

    void Texture::EnsureSubresourceContentInitialized(uint32_t baseMipLevel,
                                                      uint32_t levelCount,
                                                      uint32_t baseArrayLayer,
                                                      uint32_t layerCount,
                                                      bool isLazyClear) {
        if (!GetDevice()->IsToggleEnabled(Toggle::LazyClearResourceOnFirstUse)) {
            return;
        }
        if (!IsSubresourceContentInitialized(baseMipLevel, levelCount, baseArrayLayer,
                                             layerCount)) {
            GetDevice()->ConsumedError(ClearTexture(baseMipLevel, levelCount, baseArrayLayer,
                                                    layerCount, TextureBase::ClearValue::Zero));
            if (isLazyClear) {
                GetDevice()->IncrementLazyClearCountForTesting();
            }
            SetIsSubresourceContentInitialized(baseMipLevel, levelCount, baseArrayLayer,
                                               layerCount);
        }
    }

    // TextureView

    TextureView::TextureView(TextureBase* texture, const TextureViewDescriptor* descriptor)
        : TextureViewBase(texture, descriptor), mOwnsHandle(false) {
        mTarget = TargetForTextureViewDimension(descriptor->dimension, texture->GetSampleCount());

        if (!UsageNeedsTextureView(texture->GetUsage())) {
            mHandle = 0;
        } else if (!RequiresCreatingNewTextureView(texture, descriptor)) {
            mHandle = ToBackend(texture)->GetHandle();
        } else {
            // glTextureView() is supported on OpenGL version >= 4.3
            // TODO(jiawei.shao@intel.com): support texture view on OpenGL version <= 4.2
            const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
            mHandle = GenTexture(gl);
            const Texture* textureGL = ToBackend(texture);
            const GLFormat& glFormat = ToBackend(GetDevice())->GetGLFormat(GetFormat());
            gl.TextureView(mHandle, mTarget, textureGL->GetHandle(), glFormat.internalFormat,
                           descriptor->baseMipLevel, descriptor->mipLevelCount,
                           descriptor->baseArrayLayer, descriptor->arrayLayerCount);
            mOwnsHandle = true;
        }
    }

    TextureView::~TextureView() {
        if (mOwnsHandle) {
            ToBackend(GetDevice())->gl.DeleteTextures(1, &mHandle);
        }
    }

    GLuint TextureView::GetHandle() const {
        ASSERT(mHandle != 0);
        return mHandle;
    }

    GLenum TextureView::GetGLTarget() const {
        return mTarget;
    }

}}  // namespace dawn_native::opengl
