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

#include "dawn/tests/DawnTest.h"

namespace dawn {
namespace {

using FutureCallbackMode = std::optional<wgpu::CallbackMode>;
DAWN_TEST_PARAM_STRUCT(AdapterInfoTestParams, FutureCallbackMode);

class AdapterInfoTests : public DawnTestWithParams<AdapterInfoTestParams> {
  protected:
    void SetUp() override {
        DawnTestWithParams<AdapterInfoTestParams>::SetUp();
        // Wire only supports polling / spontaneous futures.
        DAWN_TEST_UNSUPPORTED_IF(UsesWire() && GetParam().mFutureCallbackMode &&
                                 *GetParam().mFutureCallbackMode ==
                                     wgpu::CallbackMode::WaitAnyOnly);
    }

    void RequestAdapterInfo(WGPURequestAdapterInfoCallback callback, void* userdata) {
        if (GetParam().mFutureCallbackMode == std::nullopt) {
            // Legacy RequestAdapterInfo. It should call the callback immediately.
            adapter.RequestAdapterInfo(callback, userdata);
            return;
        }

        wgpu::Future future = adapter.RequestAdapterInfo(
            {nullptr, *GetParam().mFutureCallbackMode, callback, userdata});
        switch (*GetParam().mFutureCallbackMode) {
            case wgpu::CallbackMode::WaitAnyOnly: {
                // Callback should complete as soon as poll once.
                wgpu::FutureWaitInfo waitInfo = {future};
                EXPECT_EQ(instance.WaitAny(1, &waitInfo, 0), wgpu::WaitStatus::Success);
                ASSERT_TRUE(waitInfo.completed);
                break;
            }
            case wgpu::CallbackMode::AllowSpontaneous:
                // Callback should already be called.
                break;
            case wgpu::CallbackMode::AllowProcessEvents:
                instance.ProcessEvents();
                break;
        }
    }
};

DAWN_INSTANTIATE_PREFIXED_TEST_P(Legacy,
                                 AdapterInfoTests,
                                 {D3D11Backend(), D3D12Backend(), MetalBackend(), OpenGLBackend(),
                                  OpenGLESBackend(), VulkanBackend()},
                                 {std::nullopt});

DAWN_INSTANTIATE_PREFIXED_TEST_P(Future,
                                 AdapterInfoTests,
                                 {D3D11Backend(), D3D12Backend(), MetalBackend(), OpenGLBackend(),
                                  OpenGLESBackend(), VulkanBackend()},
                                 std::initializer_list<std::optional<wgpu::CallbackMode>>{
                                     wgpu::CallbackMode::WaitAnyOnly,
                                     wgpu::CallbackMode::AllowProcessEvents,
                                     wgpu::CallbackMode::AllowSpontaneous});

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AdapterInfoTests);

// Test adapter info are not empty.
TEST_P(AdapterInfoTests, RequestAdapterInfo) {
    RequestAdapterInfo(
        [](WGPURequestAdapterInfoStatus status, const WGPUAdapterInfo* adapterInfo,
           void* userdata) {
            EXPECT_NE(adapterInfo->vendor, "");
            EXPECT_NE(adapterInfo->architecture, "");
            EXPECT_NE(adapterInfo->device, "");
            EXPECT_NE(adapterInfo->description, "");
        },
        nullptr);
}

}  // anonymous namespace
}  // namespace dawn
