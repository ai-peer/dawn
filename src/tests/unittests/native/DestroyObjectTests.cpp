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

#include <gtest/gtest.h>

#include "mocks/BindGroupLayoutMock.h"
#include "mocks/BindGroupMock.h"
#include "mocks/DeviceMock.h"
#include "tests/DawnNativeTest.h"

namespace dawn_native { namespace {

    using ::testing::ByMove;
    using ::testing::Return;

    TEST(DestroyObjectTests, BindGroupLayout) {
        DeviceMock device;
        BindGroupLayoutMock* bindGroupLayoutMock = new BindGroupLayoutMock(&device);
        EXPECT_CALL(device, CreateBindGroupLayoutImpl)
            .WillOnce(Return(ByMove(AcquireRef(bindGroupLayoutMock))));
        EXPECT_CALL(*bindGroupLayoutMock, DestroyApiObjectImpl).Times(1);

        BindGroupLayoutEntry binding = {};
        binding.binding = 0;
        binding.buffer.type = wgpu::BufferBindingType::Uniform;
        binding.buffer.minBindingSize = 4 * sizeof(float);
        BindGroupLayoutDescriptor desc = {};
        desc.entryCount = 1;
        desc.entries = &binding;

        Ref<BindGroupLayoutBase> bindGroupLayout;
        DAWN_ASSERT_AND_ASSIGN(bindGroupLayout, device.CreateBindGroupLayout(&desc));
        EXPECT_TRUE(bindGroupLayout->IsInList());
        EXPECT_TRUE(bindGroupLayout->IsCachedReference());

        bindGroupLayout->DestroyApiObject();
        EXPECT_FALSE(bindGroupLayout->IsInList());
    }

}}  // namespace dawn_native::
