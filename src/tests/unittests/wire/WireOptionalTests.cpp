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

#include "tests/unittests/wire/WireTest.h"

using namespace testing;
using namespace dawn_wire;

class WireOptionalTests : public WireTest {
  public:
    WireOptionalTests() : WireTest(true) {
    }
    ~WireOptionalTests() override = default;
};

// Test passing nullptr instead of objects - object as value version
TEST_F(WireOptionalTests, OptionalObjectValue) {
    dawnBindGroupLayoutDescriptor bglDesc;
    bglDesc.nextInChain = nullptr;
    bglDesc.numBindings = 0;
    dawnBindGroupLayout bgl = dawnDeviceCreateBindGroupLayout(device, &bglDesc);

    dawnBindGroupLayout apiBindGroupLayout = api.GetNewBindGroupLayout();
    EXPECT_CALL(api, DeviceCreateBindGroupLayout(apiDevice, _))
        .WillOnce(Return(apiBindGroupLayout));

    // The `sampler`, `textureView` and `buffer` members of a binding are optional.
    dawnBindGroupBinding binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;

    dawnBindGroupDescriptor bgDesc;
    bgDesc.nextInChain = nullptr;
    bgDesc.layout = bgl;
    bgDesc.numBindings = 1;
    bgDesc.bindings = &binding;

    dawnDeviceCreateBindGroup(device, &bgDesc);
    EXPECT_CALL(api, DeviceCreateBindGroup(
                         apiDevice, MatchesLambda([](const dawnBindGroupDescriptor* desc) -> bool {
                             return desc->nextInChain == nullptr && desc->numBindings == 1 &&
                                    desc->bindings[0].binding == 0 &&
                                    desc->bindings[0].sampler == nullptr &&
                                    desc->bindings[0].buffer == nullptr &&
                                    desc->bindings[0].textureView == nullptr;
                         })))
        .WillOnce(Return(nullptr));

    EXPECT_CALL(api, BindGroupLayoutRelease(apiBindGroupLayout));
    FlushClient();
}
