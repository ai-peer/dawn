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

#include "dawn_native/Toggles.h"
#include "mocks/BindGroupLayoutMock.h"
#include "mocks/BindGroupMock.h"
#include "mocks/BufferMock.h"
#include "mocks/DeviceMock.h"
#include "tests/DawnNativeTest.h"

namespace dawn_native { namespace {

    using ::testing::ByMove;
    using ::testing::InSequence;
    using ::testing::Return;

    TEST(DestroyObjectTests, Buffer) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BufferMock* bufferMock = new BufferMock(&device);
        EXPECT_CALL(*bufferMock, DestroyApiObjectImpl).Times(1);

        BufferDescriptor desc = {};
        Ref<BufferBase> buffer;
        EXPECT_CALL(device, CreateBufferImpl).WillOnce(Return(ByMove(AcquireRef(bufferMock))));
        DAWN_ASSERT_AND_ASSIGN(buffer, device.CreateBuffer(&desc));

        EXPECT_TRUE(buffer->IsInList());

        buffer->DestroyApiObject();
        EXPECT_FALSE(buffer->IsInList());
    }

    TEST(DestroyObjectTests, BindGroup) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BindGroupMock* bindGroupMock = new BindGroupMock(&device);
        EXPECT_CALL(*bindGroupMock, DestroyApiObjectImpl).Times(1);

        BindGroupDescriptor desc = {};
        Ref<BindGroupBase> bindGroup;
        EXPECT_CALL(device, CreateBindGroupImpl)
            .WillOnce(Return(ByMove(AcquireRef(bindGroupMock))));
        DAWN_ASSERT_AND_ASSIGN(bindGroup, device.CreateBindGroup(&desc));

        EXPECT_TRUE(bindGroup->IsInList());

        bindGroup->DestroyApiObject();
        EXPECT_FALSE(bindGroup->IsInList());
    }

    TEST(DestroyObjectTests, BindGroupLayout) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BindGroupLayoutMock* bindGroupLayoutMock = new BindGroupLayoutMock(&device);
        EXPECT_CALL(*bindGroupLayoutMock, DestroyApiObjectImpl).Times(1);

        BindGroupLayoutDescriptor desc = {};
        Ref<BindGroupLayoutBase> bindGroupLayout;
        EXPECT_CALL(device, CreateBindGroupLayoutImpl)
            .WillOnce(Return(ByMove(AcquireRef(bindGroupLayoutMock))));
        DAWN_ASSERT_AND_ASSIGN(bindGroupLayout, device.CreateBindGroupLayout(&desc));

        EXPECT_TRUE(bindGroupLayout->IsInList());
        EXPECT_TRUE(bindGroupLayout->IsCachedReference());

        bindGroupLayout->DestroyApiObject();
        EXPECT_FALSE(bindGroupLayout->IsInList());
    }

    // Destroying the device should result in all created objects being destroyed in order. Note
    // that the mock device destructor is a bit different from a real backend implementation because
    // the mock is too simple. It is only deleting the objects for now.
    TEST(DestroyObjectTests, DestroyDevice) {
        Ref<BufferBase> buffer;
        Ref<BindGroupBase> bindGroup;
        Ref<BindGroupLayoutBase> bindGroupLayout;

        {
            DeviceMock device;
            device.SetToggle(Toggle::SkipValidation, true);

            BufferMock* bufferMock = new BufferMock(&device);
            BindGroupMock* bindGroupMock = new BindGroupMock(&device);
            BindGroupLayoutMock* bindGroupLayoutMock = new BindGroupLayoutMock(&device);
            {
                InSequence seq;
                EXPECT_CALL(*bufferMock, DestroyApiObjectImpl).Times(1);
                EXPECT_CALL(*bindGroupMock, DestroyApiObjectImpl).Times(1);
                EXPECT_CALL(*bindGroupLayoutMock, DestroyApiObjectImpl).Times(1);
            }

            {
                BufferDescriptor desc = {};
                EXPECT_CALL(device, CreateBufferImpl)
                    .WillOnce(Return(ByMove(AcquireRef(bufferMock))));
                DAWN_ASSERT_AND_ASSIGN(buffer, device.CreateBuffer(&desc));
                EXPECT_TRUE(buffer->IsInList());
            }
            {
                BindGroupDescriptor desc = {};
                EXPECT_CALL(device, CreateBindGroupImpl)
                    .WillOnce(Return(ByMove(AcquireRef(bindGroupMock))));
                DAWN_ASSERT_AND_ASSIGN(bindGroup, device.CreateBindGroup(&desc));
                EXPECT_TRUE(bindGroup->IsInList());
            }
            {
                BindGroupLayoutDescriptor desc = {};
                EXPECT_CALL(device, CreateBindGroupLayoutImpl)
                    .WillOnce(Return(ByMove(AcquireRef(bindGroupLayoutMock))));
                DAWN_ASSERT_AND_ASSIGN(bindGroupLayout, device.CreateBindGroupLayout(&desc));
                EXPECT_TRUE(bindGroupLayout->IsInList());
                EXPECT_TRUE(bindGroupLayout->IsCachedReference());
            }
        }

        EXPECT_FALSE(buffer->IsInList());
        EXPECT_FALSE(bindGroup->IsInList());
        EXPECT_FALSE(bindGroupLayout->IsInList());
    }

}}  // namespace dawn_native::
