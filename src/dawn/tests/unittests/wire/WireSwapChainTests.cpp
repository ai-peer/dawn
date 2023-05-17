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

#include <memory>

#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::Return;

class WireSwapChainTests : public WireTest {
};

// Check the reflection for textures matches the SwapChainDescriptor.
TEST_F(WireSwapChainTests, CurrentTextureReflection) {
    auto CheckWithDescriptor = [&](const WGPUSwapChainDescriptor& desc) {
        WGPUSurfaceDescriptor surfaceDesc = {};
        WGPUSurface apiSurface = api.GetNewSurface();
        WGPUSurface surface = wgpuInstaceCreateSurface(instance, &surfaceDesc);
        EXPECT_CALL(api, InstaceCreateSurface(apiInstance, _)).WillOnce(Return(apiSurface));

        WGPUSwapChain apiSwapChain = api.GetNewSwapChain();
        WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &desc);
        EXPECT_CALL(api, DeviceCreateSwapChain(apiDevice, _, _)).WillOnce(Return(apiSwapChain));
        
        WGPUTexture apiTex = api.GetNewTexture();
        WGPUTexture tex = wgpuSwapChainGetCurrentTexture(swapChain);
        EXPECT_CALL(api, SwapChainGetCurrentTexture(apiSwapChain)).WillOnce(Return(apiTex));

        EXPECT_EQ(desc.width, wgpuTextureGetWidth(tex));
        EXPECT_EQ(desc.height, wgpuTextureGetHeight(tex));
        EXPECT_EQ(desc.usage, wgpuTextureGetUsage(tex));
        EXPECT_EQ(desc.format, wgpuTextureGetFormat(tex));
        EXPECT_EQ(1u, wgpuTextureGetDepthOrArrayLayers(tex));
        EXPECT_EQ(1u, wgpuTextureGetMipLevelCount(tex));
        EXPECT_EQ(1u, wgpuTextureGetSampleCount(tex));
        EXPECT_EQ(WGPUTextureDimension_2D, wgpuTextureGetDimension(tex));
    };

    WGPUSwapChainDescriptor desc = {};
    desc.usage = WGPUTextureUsage_RenderAttachment;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.width = 13;
    desc.height = 90;
    desc.presentMode = WGPUPresentMode_Mailbox;
    CheckWithDescriptor(desc);

    desc.usage = WGPUTextureUsage_StorageBinding;
    desc.format = WGPUTextureFormat_R32Float;
    desc.width = 0;
    desc.height = 20000000;
    CheckWithDescriptor(desc);
}

}  // anonymous namespace
}  // namespace dawn::wire
