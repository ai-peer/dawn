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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/DawnHelpers.h"

class DebugMarkerValidationTest : public ValidationTest {};

// Correct usage of debug markers should succeed
TEST_F(DebugMarkerValidationTest, Success) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 4, 4);

    dawn::CommandBufferBuilder builder = AssertWillBeSuccess(device.CreateCommandBufferBuilder());
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.PushDebugGroup("Event Start");
        pass.PushDebugGroup("Event Start");
        pass.InsertDebugMarker("Marker");
        pass.PopDebugGroup();
        pass.PopDebugGroup();
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
}

// A PushDebugGroup call without a following PopDebugGroup produces an error.
TEST_F(DebugMarkerValidationTest, UnbalancedPush) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 4, 4);

    dawn::CommandBufferBuilder builder = AssertWillBeError(device.CreateCommandBufferBuilder());
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.PushDebugGroup("Event Start");
        pass.PushDebugGroup("Event Start");
        pass.InsertDebugMarker("Marker");
        pass.PopDebugGroup();
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
}

// A PopDebugGroup call without a preceding PushDebugGroup produces an error.
TEST_F(DebugMarkerValidationTest, UnbalancedPop) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 4, 4);

    dawn::CommandBufferBuilder builder = AssertWillBeError(device.CreateCommandBufferBuilder());
    {
        dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
        pass.PushDebugGroup("Event Start");
        pass.InsertDebugMarker("Marker");
        pass.PopDebugGroup();
        pass.PopDebugGroup();
        pass.EndPass();
    }

    dawn::CommandBuffer commands = builder.GetResult();
}
