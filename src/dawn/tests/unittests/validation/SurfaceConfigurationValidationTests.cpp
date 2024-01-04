// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cmath>
#include <string>
#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderBundleEncoderDescriptor.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class SurfaceConfigurationValidationTest : public ValidationTest {
  public:
    void AssertSurfaceConfigurationSuccess(wgpu::Surface surface,
                                           const wgpu::SurfaceConfiguration* config) {
        surface.Configure(config);
    }
    void AssertSurfaceConfigurationError(wgpu::Surface surface,
                                         const wgpu::SurfaceConfiguration* config) {
        ASSERT_DEVICE_ERROR(surface.Configure(config));
    }

    void AssertSurfaceConfigurationError(wgpu::Surface surface,
                                         const wgpu::SurfaceConfiguration* config,
                                         testing::Matcher<std::string> errorMatcher) {
        ASSERT_DEVICE_ERROR(surface.Configure(config), errorMatcher);
    }

    wgpu::Surface CreateTestSurface() {
        wgpu::SurfaceDescriptor descriptor;
        descriptor.label = "Test Surface";
        descriptor.nextInChain = nullptr;  // TODO(dawn:2320) Which backend should I use? Should I
                                           // create a mock one for testing?
        wgpu::Surface surface = adapter.GetInstance().CreateSurface(&descriptor);
        return surface;
    }
};

// Using undefined format is not valid
TEST_F(SurfaceConfigurationValidationTest, UndefinedFormat) {
    wgpu::SurfaceConfiguration config;
    config.format = wgpu::TextureFormat::Undefined;
    AssertSurfaceConfigurationError(CreateTestSurface(), &config);
}

// Using the first capability is ok.
TEST_F(SurfaceConfigurationValidationTest, FirstCapabilities) {
    wgpu::Surface surface = CreateTestSurface();

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);

    wgpu::SurfaceConfiguration config;
    config.format = capabilities.formats[0];
    config.alphaMode = capabilities.alphaModes[0];
    config.presentMode = capabilities.presentModes[0];
    config.device = device;
    config.width = 128;
    config.height = 128;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.viewFormatCount = 1;
    config.viewFormats = &capabilities.formats[0];
    AssertSurfaceConfigurationSuccess(surface, &config);

    // TODO(dawn:2320) How come the generator did not generate SurfaceCapabilities::FreeMembers?
}

}  // anonymous namespace
}  // namespace dawn
