// Copyright 2022 The Dawn Authors
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

#include "dawn/tests/DawnNativeTest.h"

#include "dawn/native/CreatePipelineAsyncTask.h"
#include "mocks/ComputePipelineMock.h"
#include "mocks/RenderPipelineMock.h"

class CreatePipelineAsyncTaskTests : public DawnNativeTest {};

// A regression test for a null pointer issue in CreateRenderPipelineAsyncTask::Run().
// See crbug.com/dawn/1310 for more details.
TEST_F(CreatePipelineAsyncTaskTests, InitializationErrorInCreateRenderPipelineAsync) {
    class RenderPipelineMockForTest : public dawn::native::RenderPipelineMock {
      public:
        RenderPipelineMockForTest(dawn::native::DeviceBase* device) : RenderPipelineMock(device) {
        }

        dawn::native::MaybeError Initialize() {
            return DAWN_MAKE_ERROR(dawn::native::InternalErrorType::Validation,
                                   "Initialization Error");
        }
    };

    dawn::native::DeviceBase* deviceBase =
        reinterpret_cast<dawn::native::DeviceBase*>(device.Get());
    Ref<RenderPipelineMockForTest> renderPipeline =
        AcquireRef(new RenderPipelineMockForTest(deviceBase));
    EXPECT_CALL(*renderPipeline.Get(), DestroyImpl).Times(1);

    dawn::native::CreateRenderPipelineAsyncTask asyncTask(
        renderPipeline,
        [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline returnPipeline,
           const char* message, void* userdata) {
            EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Error, status);
        },
        nullptr);

    asyncTask.Run();
    device.Tick();
}

// A regression test for a null pointer issue in CreateComputePipelineAsyncTask::Run().
// See crbug.com/dawn/1310 for more details.
TEST_F(CreatePipelineAsyncTaskTests, InitializationErrorInCreateComputePipelineAsync) {
    class ComputePipelineMockForTest : public dawn::native::ComputePipelineMock {
      public:
        ComputePipelineMockForTest(dawn::native::DeviceBase* device) : ComputePipelineMock(device) {
        }

        dawn::native::MaybeError Initialize() {
            return DAWN_MAKE_ERROR(dawn::native::InternalErrorType::Validation,
                                   "Initialization Error");
        }
    };

    dawn::native::DeviceBase* deviceBase =
        reinterpret_cast<dawn::native::DeviceBase*>(device.Get());
    Ref<ComputePipelineMockForTest> computePipeline =
        AcquireRef(new ComputePipelineMockForTest(deviceBase));
    EXPECT_CALL(*computePipeline.Get(), DestroyImpl).Times(1);

    dawn::native::CreateComputePipelineAsyncTask asyncTask(
        computePipeline,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline returnPipeline,
           const char* message, void* userdata) {
            EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Error, status);
        },
        nullptr);

    asyncTask.Run();
    device.Tick();
}
