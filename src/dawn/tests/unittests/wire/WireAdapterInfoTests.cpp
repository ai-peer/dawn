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

#include <iostream>
#include <unordered_set>
#include <vector>

#include "dawn/tests/MockCallback.h"
#include "dawn/tests/unittests/wire/WireFutureTest.h"
#include "dawn/tests/unittests/wire/WireTest.h"

#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireServer.h"

#include "webgpu/webgpu_cpp.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::MockCallback;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::WithArg;

using WireAdapterInfoTestBase = WireFutureTest<WGPURequestAdapterInfoCallback,
                                               WGPURequestAdapterInfoCallbackInfo,
                                               wgpuAdapterRequestAdapterInfo,
                                               wgpuAdapterRequestAdapterInfoF>;

class WireAdapterInfoTests : public WireAdapterInfoTestBase {
  protected:
    // Overriden version of wgpuAdapterRequestAdapterInfo that defers to the API call based on the
    // test callback mode.
    void AdapterRequestAdapterInfo(const wgpu::Adapter& a, void* userdata = nullptr) {
        CallImpl(userdata, a.Get());
    }

    // Bootstrap the tests and create a fake adapter.
    void SetUp() override {
        WireAdapterInfoTestBase::SetUp();

        WGPURequestAdapterOptions options = {};
        MockCallback<WGPURequestAdapterCallback> cb;
        wgpuInstanceRequestAdapter(instance, &options, cb.Callback(), cb.MakeUserdata(this));

        // Expect the server to receive the message. Then, mock a fake reply.
        apiAdapter = api.GetNewAdapter();
        EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
            .WillOnce(InvokeWithoutArgs([&] {
                EXPECT_CALL(api, AdapterHasFeature(apiAdapter, _)).WillRepeatedly(Return(false));

                EXPECT_CALL(api, AdapterGetProperties(apiAdapter, NotNull()))
                    .WillOnce(WithArg<1>(Invoke([&](WGPUAdapterProperties* properties) {
                        *properties = {};
                        properties->vendorName = "";
                        properties->architecture = "";
                        properties->name = "";
                        properties->driverDescription = "";
                    })));

                EXPECT_CALL(api, AdapterGetLimits(apiAdapter, NotNull()))
                    .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                        *limits = {};
                        return true;
                    })));

                EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, nullptr))
                    .WillOnce(Return(0))
                    .WillOnce(Return(0));
                api.CallInstanceRequestAdapterCallback(
                    apiInstance, WGPURequestAdapterStatus_Success, apiAdapter, nullptr);
            }));
        FlushClient();

        // Expect the callback in the client.
        WGPUAdapter cAdapter;
        EXPECT_CALL(cb, Call(WGPURequestAdapterStatus_Success, NotNull(), nullptr, this))
            .WillOnce(SaveArg<1>(&cAdapter));
        FlushServer();

        EXPECT_NE(cAdapter, nullptr);
        adapter = wgpu::Adapter::Acquire(cAdapter);
    }

    void TearDown() override {
        adapter = nullptr;
        WireAdapterInfoTestBase::TearDown();
    }

    WGPUAdapter apiAdapter;
    wgpu::Adapter adapter;
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireAdapterInfoTests);

// Test that RequestAdapterInfo forwards the device information to the client.
TEST_P(WireAdapterInfoTests, RequestAdapterInfoSuccess) {
    AdapterRequestAdapterInfo(adapter);
    WGPUAdapterInfo info = {
        .vendor = "",
        .architecture = "",
        .device = "",
        .description = "",
    };

    EXPECT_CALL(api, OnAdapterRequestAdapterInfo(apiAdapter, NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            // Call the callback so the test doesn't wait indefinitely.
            api.CallAdapterRequestAdapterInfoCallback(apiAdapter,
                                                      WGPURequestAdapterInfoStatus_Success, &info);
        }));
    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call).Times(1);

        FlushCallbacks();
    });
}

}  // anonymous namespace
}  // namespace dawn::wire
