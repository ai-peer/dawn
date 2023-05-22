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

#include <limits>
#include <string>

#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class CompatValidationTest : public ValidationTest {
  protected:
    bool UseCompatibilityMode() const override { return true; }
};

TEST_F(CompatValidationTest, CanNotCreateCubeArrayTextureView) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {1, 1, 6};
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture cubeTexture = device.CreateTexture(&descriptor);

    {
        wgpu::TextureViewDescriptor cubeViewDescriptor;
        cubeViewDescriptor.dimension = wgpu::TextureViewDimension::Cube;
        cubeViewDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;

        cubeTexture.CreateView(&cubeViewDescriptor);
    }

    {
        wgpu::TextureViewDescriptor cubeArrayViewDescriptor;
        cubeArrayViewDescriptor.dimension = wgpu::TextureViewDimension::CubeArray;
        cubeArrayViewDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;

        ASSERT_DEVICE_ERROR(cubeTexture.CreateView(&cubeArrayViewDescriptor));
    }

    cubeTexture.Destroy();
}

TEST_F(CompatValidationTest, CanNotUseFragmentShaderWithSampleMask) {
    wgpu::ShaderModule moduleSampleMaskOutput = utils::CreateShaderModule(device, R"(
        @vertex fn vs() -> @builtin(position) vec4f {
            return vec4f(1);
        }
        struct Output {
            @builtin(sample_mask) mask_out: u32,
            @location(0) color : vec4f,
        }
        @fragment fn fsWithoutSampleMaskUsage() -> @location(0) vec4f {
            return vec4f(1.0, 1.0, 1.0, 1.0);
        }
        /*
        @fragment fn fsWithSampleMaskUsage() -> Output {
            var o: Output;
            // We need to make sure this sample_mask isn't optimized out even its value equals "no op".
            o.mask_out = 0xFFFFFFFFu;
            o.color = vec4f(1.0, 1.0, 1.0, 1.0);
            return o;
        }
        */
    )");

    // Check we can use a fragment shader that doesn't use sample_mask from
    // the same module as one that does.
    {
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = moduleSampleMaskOutput;
        descriptor.vertex.entryPoint = "vs";
        descriptor.cFragment.module = moduleSampleMaskOutput;
        descriptor.cFragment.entryPoint = "fsWithoutSampleMaskUsage";
        descriptor.multisample.count = 4;
        descriptor.multisample.alphaToCoverageEnabled = false;

        device.CreateRenderPipeline(&descriptor);
    }

    // Check we can not use a fragment shader that uses sample_mask.
    {
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = moduleSampleMaskOutput;
        descriptor.vertex.entryPoint = "vs";
        descriptor.cFragment.module = moduleSampleMaskOutput;
        descriptor.cFragment.entryPoint = "fsWithSampleMaskUsage";
        descriptor.multisample.count = 4;
        descriptor.multisample.alphaToCoverageEnabled = false;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

}  // anonymous namespace
}  // namespace dawn
