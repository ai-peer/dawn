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

#include <gtest/gtest.h>

#include <string_view>

#include "dawn/tests/MockCallback.h"
#include "dawn/webgpu_cpp.h"
#include "mocks/BufferMock.h"
#include "mocks/ComputePipelineMock.h"
#include "mocks/DawnMockTest.h"
#include "mocks/DeviceMock.h"
#include "mocks/PipelineLayoutMock.h"
#include "mocks/RenderPipelineMock.h"
#include "mocks/TextureMock.h"

namespace dawn::native {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using testing::MockCallback;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;

static constexpr char kOomErrorMessage[] = "Out of memory error";

class ErrorMaskingTests : public DawnMockTest {
  public:
    ErrorMaskingTests() : DawnMockTest() {
        // Skipping validation on descriptors as coverage for validation is already present.
        mDeviceMock->ForceSetToggleForTesting(Toggle::SkipValidation, true);

        device.SetDeviceLostCallback(mDeviceLostCb.Callback(), mDeviceLostCb.MakeUserdata(this));
        device.SetUncapturedErrorCallback(mDeviceErrorCb.Callback(),
                                          mDeviceErrorCb.MakeUserdata(this));
    }

    ~ErrorMaskingTests() override { device = nullptr; }

  protected:
    // Device mock callbacks used throughout the tests.
    StrictMock<MockCallback<wgpu::DeviceLostCallback>> mDeviceLostCb;
    StrictMock<MockCallback<wgpu::ErrorCallback>> mDeviceErrorCb;
};

//
// Exercise APIs where OOM errors cause a device lost.
//

TEST_F(ErrorMaskingTests, QueueSubmit) {
    EXPECT_CALL(*(mDeviceMock->GetQueueMock()), SubmitImpl)
        .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));

    // Expect the device lost because of the error.
    EXPECT_CALL(mDeviceLostCb,
                Call(WGPUDeviceLostReason_Undefined, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    device.GetQueue().Submit(0, nullptr);
}

TEST_F(ErrorMaskingTests, QueueWriteBuffer) {
    BufferDescriptor desc = {};
    desc.size = 1;
    desc.usage = wgpu::BufferUsage::CopyDst;
    BufferMock* bufferMock = new BufferMock(mDeviceMock, &desc);
    wgpu::Buffer buffer = wgpu::Buffer::Acquire(ToAPI(bufferMock));

    EXPECT_CALL(*(mDeviceMock->GetQueueMock()), WriteBufferImpl)
        .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));

    // Expect the device lost because of the error.
    EXPECT_CALL(mDeviceLostCb,
                Call(WGPUDeviceLostReason_Undefined, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    constexpr uint8_t data = 8;
    device.GetQueue().WriteBuffer(buffer, 0, &data, 0);
}

TEST_F(ErrorMaskingTests, QueueWriteTexture) {
    TextureDescriptor desc = {};
    desc.size.width = 1;
    desc.size.height = 1;
    desc.usage = wgpu::TextureUsage::CopyDst;
    desc.format = wgpu::TextureFormat::RGBA8Unorm;
    TextureMock* textureMock =
        new TextureMock(mDeviceMock, &desc, TextureBase::TextureState::OwnedInternal);
    wgpu::Texture texture = wgpu::Texture::Acquire(ToAPI(textureMock));

    EXPECT_CALL(*(mDeviceMock->GetQueueMock()), WriteTextureImpl)
        .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));

    // Expect the device lost because of the error.
    EXPECT_CALL(mDeviceLostCb,
                Call(WGPUDeviceLostReason_Undefined, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    constexpr uint8_t data[] = {1, 2, 4, 8};
    wgpu::ImageCopyTexture dest = {};
    dest.texture = texture;
    wgpu::TextureDataLayout layout = {};
    wgpu::Extent3D size = {};
    size.width = 1;
    size.height = 1;
    device.GetQueue().WriteTexture(&dest, &data, 4, &layout, &size);
}

// TEST_F(ErrorMaskingTests, CreateComputePipeline) {
//     // Use the mock render pipeline to cause a failure when initializing. Note that we use an
//     // explicit pipeline layout to avoid trying to create a default one.
//     PipelineLayoutMock* pipelineLayoutMock = new PipelineLayoutMock(mDeviceMock);

//     ComputePipelineMock* computePipelineMock = new ComputePipelineMock(mDeviceMock);
//     EXPECT_CALL(*computePipelineMock, Initialize)
//         .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));
//     EXPECT_CALL(*mDeviceMock, CreateUninitializedComputePipelineImpl)
//         .WillOnce(Return(ByMove(AcquireRef(computePipelineMock))));

//     // Expect the device lost because of the error.
//     EXPECT_CALL(mDeviceLostCb,
//                 Call(WGPUDeviceLostReason_Undefined, HasSubstr(kOomErrorMessage), this))
//         .Times(1);

//     wgpu::ComputePipelineDescriptor desc = {};
//     desc.layout = wgpu::PipelineLayout::Acquire(ToAPI(pipelineLayoutMock));
//     device.CreateComputePipeline(&desc);
// }

// TEST_F(ErrorMaskingTests, CreateRenderPipeline) {
//     // Use the mock render pipeline to cause a failure when initializing. Note that we use an
//     // explicit pipeline layout to avoid trying to create a default one.
//     PipelineLayoutMock* pipelineLayoutMock = new PipelineLayoutMock(mDeviceMock);

//     RenderPipelineMock* renderPipelineMock = new RenderPipelineMock(mDeviceMock);
//     EXPECT_CALL(*renderPipelineMock, Initialize)
//         .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));
//     EXPECT_CALL(*mDeviceMock, CreateUninitializedRenderPipelineImpl)
//         .WillOnce(Return(ByMove(AcquireRef(renderPipelineMock))));

//     // Expect the device lost because of the error.
//     EXPECT_CALL(mDeviceLostCb,
//                 Call(WGPUDeviceLostReason_Undefined, HasSubstr(kOomErrorMessage), this))
//         .Times(1);

//     wgpu::RenderPipelineDescriptor desc = {};
//     desc.layout = wgpu::PipelineLayout::Acquire(ToAPI(pipelineLayoutMock));
//     device.CreateRenderPipeline(&desc);
// }

//
// Excercise async APIs where OOM errors do NOT currently cause a device lost.
//

// TEST_F(ErrorMaskingTests, CreateComputePipelineAsync) {
//     // Use the mock render pipeline to cause a failure when initializing. Note that we use an
//     // explicit pipeline layout to avoid trying to create a default one.
//     PipelineLayoutMock* pipelineLayoutMock = new PipelineLayoutMock(mDeviceMock);

//     ComputePipelineMock* computePipelineMock = new ComputePipelineMock(mDeviceMock);
//     EXPECT_CALL(*computePipelineMock, Initialize)
//         .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));
//     EXPECT_CALL(*mDeviceMock, CreateUninitializedComputePipelineImpl)
//         .WillOnce(Return(ByMove(AcquireRef(computePipelineMock))));

//     MockCallback<wgpu::CreateComputePipelineAsyncCallback> cb;
//     EXPECT_CALL(cb, Call(WGPUCreatePipelineAsyncStatus_Error, _, HasSubstr(kOomErrorMessage),
//     this))
//         .Times(1);

//     wgpu::ComputePipelineDescriptor desc = {};
//     desc.layout = wgpu::PipelineLayout::Acquire(ToAPI(pipelineLayoutMock));
//     device.CreateComputePipelineAsync(&desc, cb.Callback(), cb.MakeUserdata(this));
//     device.Tick();

//     // Device lost should only happen because of destruction.
//     EXPECT_CALL(mDeviceLostCb, Call(WGPUDeviceLostReason_Destroyed, _, this)).Times(1);
// }

// TEST_F(ErrorMaskingTests, CreateRenderPipelineAsync) {
//     // Use the mock render pipeline to cause a failure when initializing. Note that we use an
//     // explicit pipeline layout to avoid trying to create a default one.
//     PipelineLayoutMock* pipelineLayoutMock = new PipelineLayoutMock(mDeviceMock);

//     RenderPipelineMock* renderPipelineMock = new RenderPipelineMock(mDeviceMock);
//     EXPECT_CALL(*renderPipelineMock, Initialize)
//         .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));
//     EXPECT_CALL(*mDeviceMock, CreateUninitializedRenderPipelineImpl)
//         .WillOnce(Return(ByMove(AcquireRef(renderPipelineMock))));

//     MockCallback<wgpu::CreateRenderPipelineAsyncCallback> cb;
//     EXPECT_CALL(cb, Call(WGPUCreatePipelineAsyncStatus_Error, _, HasSubstr(kOomErrorMessage),
//     this))
//         .Times(1);

//     wgpu::RenderPipelineDescriptor desc = {};
//     desc.layout = wgpu::PipelineLayout::Acquire(ToAPI(pipelineLayoutMock));
//     device.CreateRenderPipelineAsync(&desc, cb.Callback(), cb.MakeUserdata(this));
//     device.Tick();

//     // Device lost should only happen because of destruction.
//     EXPECT_CALL(mDeviceLostCb, Call(WGPUDeviceLostReason_Destroyed, _, this)).Times(1);
// }

//
// Excercise APIs where OOM error are allowed and surfaced.
//

TEST_F(ErrorMaskingTests, CreateBuffer) {
    EXPECT_CALL(*mDeviceMock, CreateBufferImpl)
        .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));

    // Expect the OOM error.
    EXPECT_CALL(mDeviceErrorCb, Call(WGPUErrorType_OutOfMemory, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    wgpu::BufferDescriptor desc = {};
    device.CreateBuffer(&desc);

    // Device lost should only happen because of destruction.
    EXPECT_CALL(mDeviceLostCb, Call(WGPUDeviceLostReason_Destroyed, _, this)).Times(1);
}

TEST_F(ErrorMaskingTests, CreateTexture) {
    EXPECT_CALL(*mDeviceMock, CreateTextureImpl)
        .WillOnce(Return(DAWN_OUT_OF_MEMORY_ERROR(kOomErrorMessage)));

    // Expect the OOM error.
    EXPECT_CALL(mDeviceErrorCb, Call(WGPUErrorType_OutOfMemory, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    wgpu::TextureDescriptor desc = {};
    device.CreateTexture(&desc);

    // Device lost should only happen because of destruction.
    EXPECT_CALL(mDeviceLostCb, Call(WGPUDeviceLostReason_Destroyed, _, this)).Times(1);
}

TEST_F(ErrorMaskingTests, InjectError) {
    // Expect the OOM error.
    EXPECT_CALL(mDeviceErrorCb, Call(WGPUErrorType_OutOfMemory, HasSubstr(kOomErrorMessage), this))
        .Times(1);

    device.InjectError(wgpu::ErrorType::OutOfMemory, kOomErrorMessage);

    // Device lost should only happen because of destruction.
    EXPECT_CALL(mDeviceLostCb, Call(WGPUDeviceLostReason_Destroyed, _, this)).Times(1);
}

}  // namespace
}  // namespace dawn::native
