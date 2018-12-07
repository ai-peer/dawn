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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/ComboBlendStateDescriptor.h"

class BlendStateValidationTest : public ValidationTest {
};

// Test cases where creation should succeed
TEST_F(BlendStateValidationTest, CreationSuccess) {
    // Success for setting all properties
    {
        dawn::BlendDescriptor blend;
        blend.operation = dawn::BlendOperation::Add;
        blend.srcFactor = dawn::BlendFactor::One;
        blend.dstFactor = dawn::BlendFactor::One;

        utils::ComboBlendStateDescriptor descriptor(device);
        descriptor.blendEnabled = true;
        descriptor.alphaBlend = blend;
        descriptor.colorBlend = blend;
        descriptor.colorWriteMask = dawn::ColorWriteMask::Red;

        dawn::BlendState state = device.CreateBlendState(&descriptor);
    }

    // Success for empty builder
    {
        utils::ComboBlendStateDescriptor descriptor(device);
        dawn::BlendState state = device.CreateBlendState(&descriptor);
    }
}
