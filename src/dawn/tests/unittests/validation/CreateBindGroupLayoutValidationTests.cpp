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

#include "dawn/tests/unittests/validation/ValidationTest.h"

#include "dawn/native/BindGroupLayout.h"

namespace dawn {
namespace {

class CreateBindGroupLayoutTests : public ValidationTest {};

// Tests that creating a bind group layout with a static sampler raises an error
// if the required feature is not enabled.
TEST_F(CreateBindGroupLayoutTests, StaticSamplerNotSupportedWithoutFeatureEnabled) {
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::BindGroupLayoutEntry binding = {};
    binding.binding = 0;
    binding.sampler.type = wgpu::SamplerBindingType::Filtering;
    wgpu::StaticSampler staticSampler = {};
    binding.sampler.nextInChain = &staticSampler;

    wgpu::BindGroupLayoutDescriptor desc = {};
    desc.entryCount = 1;
    desc.entries = &binding;

    ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&desc));
}

class CreateBindGroupLayoutWithStaticSamplersTests : public CreateBindGroupLayoutTests {
    WGPUDevice CreateTestDevice(native::Adapter dawnAdapter,
                                wgpu::DeviceDescriptor descriptor) override {
        wgpu::FeatureName requiredFeatures[1] = {wgpu::FeatureName::StaticSamplers};
        descriptor.requiredFeatures = requiredFeatures;
        descriptor.requiredFeatureCount = 1;
        return dawnAdapter.CreateDevice(&descriptor);
    }
};

// Tests that creating a bind group layout with a static sampler succeeds if the
// required feature is enabled.
TEST_F(CreateBindGroupLayoutWithStaticSamplersTests, StaticSamplerSupportedWhenFeatureEnabled) {
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::BindGroupLayoutEntry binding = {};
    binding.binding = 0;
    binding.sampler.type = wgpu::SamplerBindingType::Filtering;
    wgpu::StaticSampler staticSampler = {};
    binding.sampler.nextInChain = &staticSampler;

    wgpu::BindGroupLayoutDescriptor desc = {};
    desc.entryCount = 1;
    desc.entries = &binding;

    device.CreateBindGroupLayout(&desc);
}

}  // anonymous namespace
}  // namespace dawn
