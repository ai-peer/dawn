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

#include "tests/DawnTest.h"

#include "dawn_native/OpenGLBackend.h"
#include "dawn_native/opengl/DeviceGL.h"
#include "src/common/DynamicLib.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include <EGL/egl.h>

namespace {

    class EGLFunctions {
      public:
        EGLFunctions() {
            if (!mlibEGL.Open("libEGL.so")) {
                fprintf(stderr, "Couldn't load libEGL\n");
                exit(1);
            }
            Initialize = reinterpret_cast<PFNEGLINITIALIZEPROC>(mlibEGL.GetProc("eglInitialize"));
            if (!Initialize) {
                fprintf(stderr, "Couldn't find eglInitialize\n");
                exit(1);
            }
            CreateImage =
                reinterpret_cast<PFNEGLCREATEIMAGEPROC>(mlibEGL.GetProc("eglCreateImage"));
            if (!CreateImage) {
                fprintf(stderr, "Couldn't find eglCreateImage\n");
                exit(1);
            }
            DestroyImage =
                reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(mlibEGL.GetProc("eglDestroyImage"));
            if (!DestroyImage) {
                fprintf(stderr, "Couldn't find eglDestroyImage\n");
                exit(1);
            }
            GetCurrentContext = reinterpret_cast<PFNEGLGETCURRENTCONTEXTPROC>(
                mlibEGL.GetProc("eglGetCurrentContext"));
            if (!GetCurrentContext) {
                fprintf(stderr, "Couldn't find eglGetCurrentContext\n");
                exit(1);
            }
            GetCurrentDisplay = reinterpret_cast<PFNEGLGETCURRENTDISPLAYPROC>(
                mlibEGL.GetProc("eglGetCurrentDisplay"));
            if (!GetCurrentDisplay) {
                fprintf(stderr, "Couldn't find eglGetCurrentDisplay\n");
                exit(1);
            }
        }
        PFNEGLINITIALIZEPROC Initialize;
        PFNEGLCREATEIMAGEPROC CreateImage;
        PFNEGLDESTROYIMAGEPROC DestroyImage;
        PFNEGLGETCURRENTCONTEXTPROC GetCurrentContext;
        PFNEGLGETCURRENTDISPLAYPROC GetCurrentDisplay;

      private:
        DynamicLib mlibEGL;
    };

    class ScopedEGLImage {
      public:
        ScopedEGLImage(const EGLFunctions& eglFunctions,
                       EGLDisplay display,
                       EGLImage image,
                       GLuint texture)
            : egl(eglFunctions), mDisplay(display), mImage(image), mTexture(texture) {
        }

        ~ScopedEGLImage() {
            if (mImage != nullptr) {
                egl.DestroyImage(mDisplay, mImage);
            }
        }

        EGLImage getImage() const {
            return mImage;
        }

        GLuint getTexture() const {
            return mTexture;
        }

        ScopedEGLImage(ScopedEGLImage&& other) : egl(other.egl) {
            if (mImage != nullptr) {
                egl.DestroyImage(mDisplay, mImage);
            }
            mImage = other.mImage;
            other.mImage = nullptr;
        }

        ScopedEGLImage& operator=(ScopedEGLImage&& other) {
            if (mImage != nullptr) {
                egl.DestroyImage(mDisplay, mImage);
            }
            mImage = other.mImage;
            other.mImage = nullptr;

            return *this;
        }

        ScopedEGLImage(const ScopedEGLImage&) = delete;
        ScopedEGLImage& operator=(const ScopedEGLImage&) = delete;

      private:
        const EGLFunctions& egl;
        EGLDisplay mDisplay = nullptr;
        EGLImage mImage = nullptr;
        GLuint mTexture = 0;
    };

    ScopedEGLImage CreateEGLImage(const EGLFunctions& egl,
                                  wgpu::Device device,
                                  uint32_t width,
                                  uint32_t height,
                                  GLenum internalFormat,
                                  GLenum format,
                                  GLenum type,
                                  void* data,
                                  size_t size) {
        dawn_native::opengl::Device* openglDevice =
            reinterpret_cast<dawn_native::opengl::Device*>(device.Get());
        const dawn_native::opengl::OpenGLFunctions& gl = openglDevice->gl;
        GLuint tex;
        gl.GenTextures(1, &tex);
        gl.BindTexture(GL_TEXTURE_2D, tex);
        gl.TexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);
        EGLAttrib attribs[1] = {EGL_NONE};
        EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(tex);
        EGLDisplay dpy = egl.GetCurrentDisplay();
        EGLContext ctx = egl.GetCurrentContext();
        EGLImage eglImage = egl.CreateImage(dpy, ctx, EGL_GL_TEXTURE_2D, buffer, attribs);
        EXPECT_NE(nullptr, eglImage);

        return ScopedEGLImage(egl, dpy, eglImage, tex);
    }

    class EGLImageTestBase : public DawnTest {
      public:
        wgpu::Texture WrapEGLImage(const wgpu::TextureDescriptor* descriptor,
                                   EGLImage eglImage) {
            dawn_native::opengl::ExternalImageDescriptorEGLImage externDesc;
            externDesc.cTextureDescriptor =
                reinterpret_cast<const WGPUTextureDescriptor*>(descriptor);
            externDesc.image = eglImage;
            WGPUTexture texture =
                dawn_native::opengl::WrapExternalEGLImage(device.Get(), &externDesc);
            return wgpu::Texture::Acquire(texture);
        }
        EGLFunctions egl;
    };

}  // anonymous namespace

#if 0
// A small fixture used to initialize default data for the EGLImage validation tests.
// These tests are skipped if the harness is using the wire.
class EGLImageValidationTests : public EGLImageTestBase {
  public:
    EGLImageValidationTests() {
        defaultEGLImage = CreateEGLImage(device, 10, 10, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, 0);

        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.format = wgpu::TextureFormat::BGRA8Unorm;
        descriptor.size = {10, 10, 1};
        descriptor.sampleCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    }

  protected:
    wgpu::TextureDescriptor descriptor;
    ScopedEGLImage defaultEGLImage;
};

// Test a successful wrapping of an EGLImage in a texture
TEST_P(EGLImageValidationTests, Success) {
    DAWN_SKIP_TEST_IF(UsesWire());
    wgpu::Texture texture = WrapEGLImage(&descriptor, defaultEGLImage.getImage());
    ASSERT_NE(texture.Get(), nullptr);
}

// Test an error occurs if the texture descriptor is invalid
TEST_P(EGLImageValidationTests, InvalidTextureDescriptor) {
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::ChainedStruct chainedDescriptor;
    descriptor.nextInChain = &chainedDescriptor;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the plane is too large
TEST_P(EGLImageValidationTests, PlaneTooLarge) {
    DAWN_SKIP_TEST_IF(UsesWire());
    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor dimension isn't 2D
// TODO(cwallez@chromium.org): Reenable when 1D or 3D textures are implemented
TEST_P(EGLImageValidationTests, DISABLED_InvalidTextureDimension) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.dimension = wgpu::TextureDimension::e2D;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor mip level count isn't 1
TEST_P(EGLImageValidationTests, InvalidMipLevelCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.mipLevelCount = 2;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor depth isn't 1
TEST_P(EGLImageValidationTests, InvalidDepth) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.size.depthOrArrayLayers = 2;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor sample count isn't 1
TEST_P(EGLImageValidationTests, InvalidSampleCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.sampleCount = 4;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor width doesn't match the surface's
TEST_P(EGLImageValidationTests, InvalidWidth) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.size.width = 11;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor height doesn't match the surface's
TEST_P(EGLImageValidationTests, InvalidHeight) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.size.height = 11;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor format isn't compatible with the EGLImage's
TEST_P(EGLImageValidationTests, InvalidFormat) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.format = wgpu::TextureFormat::R8Unorm;

    ASSERT_DEVICE_ERROR(wgpu::Texture texture =
                            WrapEGLImage(&descriptor, defaultEGLImage.getImage()));
    ASSERT_EQ(texture.Get(), nullptr);
}
#endif

// Fixture to test using EGLImages through different usages.
// These tests are skipped if the harness is using the wire.
class EGLImageUsageTests : public EGLImageTestBase {
  public:
    // Test that clearing using BeginRenderPass writes correct data in the eglImage.
    void DoClearTest(EGLImage eglImage,
                     GLuint texture,
                     wgpu::TextureFormat format,
                     GLenum glFormat,
                     GLenum glType,
                     void* data,
                     size_t dataSize) {
        dawn_native::opengl::Device* openglDevice =
            reinterpret_cast<dawn_native::opengl::Device*>(device.Get());
        const dawn_native::opengl::OpenGLFunctions& gl = openglDevice->gl;

        // Get a texture view for the eglImage
        wgpu::TextureDescriptor textureDescriptor;
        textureDescriptor.dimension = wgpu::TextureDimension::e2D;
        textureDescriptor.format = format;
        textureDescriptor.size = {1, 1, 1};
        textureDescriptor.sampleCount = 1;
        textureDescriptor.mipLevelCount = 1;
        textureDescriptor.usage = wgpu::TextureUsage::RenderAttachment;
        wgpu::Texture eglImageTexture = WrapEGLImage(&textureDescriptor, eglImage);

        wgpu::TextureView eglImageView = eglImageTexture.CreateView();

        utils::ComboRenderPassDescriptor renderPassDescriptor({eglImageView}, {});
        renderPassDescriptor.cColorAttachments[0].clearColor = {1 / 255.0f, 2 / 255.0f, 3 / 255.0f,
                                                                4 / 255.0f};

        // Execute commands to clear the eglImage
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.EndPass();

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Check the correct data was written
        std::vector<uint8_t> result(dataSize);
        GLuint fbo;
        gl.GenFramebuffers(1, &fbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture,
                                0);
        gl.ReadPixels(0, 0, 1, 1, glFormat, glType, result.data());
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.DeleteFramebuffers(1, &fbo);
        ASSERT_EQ(0, memcmp(result.data(), data, dataSize));
    }
};

#if 0
// Test clearing a R8 EGLImage
TEST_P(EGLImageUsageTests, ClearR8EGLImage) {
    DAWN_SKIP_TEST_IF(UsesWire());
    ScopedEGLImage eglImage =
        CreateEGLImage(device, 1, 1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, nullptr, 0);

    uint8_t data = 0x01;
    DoClearTest(eglImage.getImage(), eglImage.getTexture(), wgpu::TextureFormat::R8Unorm, GL_R8, GL_UNSIGNED_BYTE, &data, sizeof(data));
}

// Test clearing a RG8 EGLImage
TEST_P(EGLImageUsageTests, ClearRG8EGLImage) {
    DAWN_SKIP_TEST_IF(UsesWire());
    ScopedEGLImage eglImage =
        CreateEGLImage(device, 1, 1, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, nullptr, 0);

    uint16_t data = 0x0201;
    DoClearTest(eglImage.getImage(), eglImage.getTexture(), wgpu::TextureFormat::RG8Unorm, GL_RG8, GL_UNSIGNED_BYTE, &data, sizeof(data));
}

#    if 0
// Test clearing a BGRA8 EGLImage
TEST_P(EGLImageUsageTests, ClearBGRA8EGLImage) {
    DAWN_SKIP_TEST_IF(UsesWire());
    ScopedEGLImage eglImage =
        CreateEGLImage(device, 1, 1, GL_BGRA8, GL_BGRA, GL_UNSIGNED_BYTE, nullptr, 0);

    uint32_t data = 0x04010203;
    DoClearTest(eglImage.getImage(), wgpu::TextureFormat::BGRA8Unorm, &data, sizeof(data));
}
#    endif
#endif

// Test clearing an RGBA8 EGLImage
TEST_P(EGLImageUsageTests, ClearRGBA8EGLImage) {
    DAWN_SKIP_TEST_IF(UsesWire());
    ScopedEGLImage eglImage =
        CreateEGLImage(egl, device, 1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, 0);

    uint32_t data = 0x04030201;
    DoClearTest(eglImage.getImage(), eglImage.getTexture(), wgpu::TextureFormat::RGBA8Unorm,
                GL_RGBA, GL_UNSIGNED_BYTE, &data, sizeof(data));
}

// DAWN_INSTANTIATE_TEST(EGLImageValidationTests, OpenGLESBackend());
DAWN_INSTANTIATE_TEST(EGLImageUsageTests, OpenGLESBackend());
