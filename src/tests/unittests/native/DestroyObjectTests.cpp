// Copyright 2021 The Dawn Authors
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
#include "mocks/SamplerMock.h"
#include "tests/DawnNativeTest.h"

namespace dawn_native { namespace {

    using ::testing::ByMove;
    using ::testing::InSequence;
    using ::testing::Return;

    TEST(DestroyObjectTests, BufferExplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BufferMock* bufferMock = new BufferMock(&device);
        EXPECT_CALL(*bufferMock, DestroyApiObjectImpl).Times(1);

        BufferDescriptor desc = {};
        Ref<BufferBase> buffer;
        EXPECT_CALL(device, CreateBufferImpl).WillOnce(Return(ByMove(AcquireRef(bufferMock))));
        DAWN_ASSERT_AND_ASSIGN(buffer, device.CreateBuffer(&desc));

        EXPECT_TRUE(buffer->IsAlive());

        buffer->DestroyApiObject();
        EXPECT_FALSE(buffer->IsAlive());
    }

    // If the reference count on API objects reach 0, they should delete themselves. Note that GTest
    // will also complain if there is a memory leak.
    TEST(DestroyObjectTests, BufferImplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BufferMock* bufferMock = new BufferMock(&device);
        EXPECT_CALL(*bufferMock, DestroyApiObjectImpl).Times(1);

        {
            BufferDescriptor desc = {};
            Ref<BufferBase> buffer;
            EXPECT_CALL(device, CreateBufferImpl).WillOnce(Return(ByMove(AcquireRef(bufferMock))));
            DAWN_ASSERT_AND_ASSIGN(buffer, device.CreateBuffer(&desc));

            EXPECT_TRUE(buffer->IsAlive());
        }
    }

    TEST(DestroyObjectTests, BindGroupExplicit) {
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

        EXPECT_TRUE(bindGroup->IsAlive());

        bindGroup->DestroyApiObject();
        EXPECT_FALSE(bindGroup->IsAlive());
    }

    // If the reference count on API objects reach 0, they should delete themselves. Note that GTest
    // will also complain if there is a memory leak.
    TEST(DestroyObjectTests, BindGroupImplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BindGroupMock* bindGroupMock = new BindGroupMock(&device);
        EXPECT_CALL(*bindGroupMock, DestroyApiObjectImpl).Times(1);

        {
            BindGroupDescriptor desc = {};
            Ref<BindGroupBase> bindGroup;
            EXPECT_CALL(device, CreateBindGroupImpl)
                .WillOnce(Return(ByMove(AcquireRef(bindGroupMock))));
            DAWN_ASSERT_AND_ASSIGN(bindGroup, device.CreateBindGroup(&desc));

            EXPECT_TRUE(bindGroup->IsAlive());
        }
    }

    TEST(DestroyObjectTests, BindGroupLayoutExplicit) {
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

        EXPECT_TRUE(bindGroupLayout->IsAlive());
        EXPECT_TRUE(bindGroupLayout->IsCachedReference());

        bindGroupLayout->DestroyApiObject();
        EXPECT_FALSE(bindGroupLayout->IsAlive());
    }

    // If the reference count on API objects reach 0, they should delete themselves. Note that GTest
    // will also complain if there is a memory leak.
    TEST(DestroyObjectTests, BindGroupLayoutImplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BindGroupLayoutMock* bindGroupLayoutMock = new BindGroupLayoutMock(&device);
        EXPECT_CALL(*bindGroupLayoutMock, DestroyApiObjectImpl).Times(1);

        {
            BindGroupLayoutDescriptor desc = {};
            Ref<BindGroupLayoutBase> bindGroupLayout;
            EXPECT_CALL(device, CreateBindGroupLayoutImpl)
                .WillOnce(Return(ByMove(AcquireRef(bindGroupLayoutMock))));
            DAWN_ASSERT_AND_ASSIGN(bindGroupLayout, device.CreateBindGroupLayout(&desc));

            EXPECT_TRUE(bindGroupLayout->IsAlive());
            EXPECT_TRUE(bindGroupLayout->IsCachedReference());
        }
    }

    TEST(DestroyObjectTests, SamplerExplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        SamplerMock* samplerMock = new SamplerMock(&device);
        EXPECT_CALL(*samplerMock, DestroyApiObjectImpl).Times(1);

        SamplerDescriptor desc = {};
        Ref<SamplerBase> sampler;
        EXPECT_CALL(device, CreateSamplerImpl).WillOnce(Return(ByMove(AcquireRef(samplerMock))));
        DAWN_ASSERT_AND_ASSIGN(sampler, device.CreateSampler(&desc));

        EXPECT_TRUE(sampler->IsAlive());
        EXPECT_TRUE(sampler->IsCachedReference());

        sampler->DestroyApiObject();
        EXPECT_FALSE(sampler->IsAlive());
    }

    // If the reference count on API objects reach 0, they should delete themselves. Note that GTest
    // will also complain if there is a memory leak.
    TEST(DestroyObjectTests, SamplerImplicit) {
        // Skipping validation on descriptors as coverage for validation is already present.
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        SamplerMock* samplerMock = new SamplerMock(&device);
        EXPECT_CALL(*samplerMock, DestroyApiObjectImpl).Times(1);

        {
            SamplerDescriptor desc = {};
            Ref<SamplerBase> sampler;
            EXPECT_CALL(device, CreateSamplerImpl)
                .WillOnce(Return(ByMove(AcquireRef(samplerMock))));
            DAWN_ASSERT_AND_ASSIGN(sampler, device.CreateSampler(&desc));

            EXPECT_TRUE(sampler->IsAlive());
            EXPECT_TRUE(sampler->IsCachedReference());
        }
    }

    // Destroying the objects on the device should result in all created objects being destroyed in
    // order.
    TEST(DestroyObjectTests, DestroyObjects) {
        DeviceMock device;
        device.SetToggle(Toggle::SkipValidation, true);

        BufferMock* bufferMock = new BufferMock(&device);
        BindGroupMock* bindGroupMock = new BindGroupMock(&device);
        BindGroupLayoutMock* bindGroupLayoutMock = new BindGroupLayoutMock(&device);
        {
            InSequence seq;
            EXPECT_CALL(*bindGroupMock, DestroyApiObjectImpl).Times(1);
            EXPECT_CALL(*bindGroupLayoutMock, DestroyApiObjectImpl).Times(1);
            EXPECT_CALL(*bufferMock, DestroyApiObjectImpl).Times(1);
        }

        Ref<BufferBase> buffer;
        {
            BufferDescriptor desc = {};
            EXPECT_CALL(device, CreateBufferImpl).WillOnce(Return(ByMove(AcquireRef(bufferMock))));
            DAWN_ASSERT_AND_ASSIGN(buffer, device.CreateBuffer(&desc));
            EXPECT_TRUE(buffer->IsAlive());
        }

        Ref<BindGroupLayoutBase> bindGroupLayout;
        {
            BindGroupLayoutDescriptor desc = {};
            EXPECT_CALL(device, CreateBindGroupLayoutImpl)
                .WillOnce(Return(ByMove(AcquireRef(bindGroupLayoutMock))));
            DAWN_ASSERT_AND_ASSIGN(bindGroupLayout, device.CreateBindGroupLayout(&desc));
            EXPECT_TRUE(bindGroupLayout->IsAlive());
            EXPECT_TRUE(bindGroupLayout->IsCachedReference());
        }

        Ref<BindGroupBase> bindGroup;
        {
            BindGroupDescriptor desc = {};
            EXPECT_CALL(device, CreateBindGroupImpl)
                .WillOnce(Return(ByMove(AcquireRef(bindGroupMock))));
            DAWN_ASSERT_AND_ASSIGN(bindGroup, device.CreateBindGroup(&desc));
            EXPECT_TRUE(bindGroup->IsAlive());
        }

        device.DestroyObjects();
        EXPECT_FALSE(buffer->IsAlive());
        EXPECT_FALSE(bindGroupLayout->IsAlive());
        EXPECT_FALSE(bindGroup->IsAlive());
    }

}}  // namespace dawn_native::
