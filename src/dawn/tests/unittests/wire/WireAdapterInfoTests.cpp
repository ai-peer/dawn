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

#include "dawn/tests/unittests/wire/WireFutureTest.h"
#include "dawn/tests/unittests/wire/WireTest.h"

#include "dawn/wire/WireClient.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NotNull;
using testing::WithArg;

using WireAdapterInfoTestBase = WireFutureTest<WGPURequestAdapterInfoCallback,
                                               WGPURequestAdapterInfoCallbackInfo,
                                               wgpuAdapterRequestAdapterInfo,
                                               wgpuAdapterRequestAdapterInfoF>;

class WireAdapterInfoTests : public WireAdapterInfoTestBase {
  protected:
    // Overridden version of wgpuAdapterRequestAdapterInfo that defers to the API call based on the
    // test callback mode.
    void AdapterRequestAdapterInfo(const wgpu::Adapter& a, void* userdata = nullptr) {
        CallImpl(userdata, a.Get());
    }
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireAdapterInfoTests);

// Test that RequestAdapterInfo forwards the device information to the client.
TEST_P(WireAdapterInfoTests, RequestAdapterInfoSuccess) {
    AdapterRequestAdapterInfo(adapter);
    WGPUAdapterInfo fakeAdapterInfo = {
        .nextInChain = nullptr,
        .vendor = "fake-vendor",
        .architecture = "fake-architecture",
        .device = "fake-device",
        .description = "fake-description",
    };

    EXPECT_CALL(api, OnAdapterRequestAdapterInfo(apiAdapter, _)).WillOnce(InvokeWithoutArgs([&] {
        // Call the callback so the test doesn't wait indefinitely.
        api.CallAdapterRequestAdapterInfoCallback(apiAdapter, WGPURequestAdapterInfoStatus_Success,
                                                  &fakeAdapterInfo);
    }));

    FlushClient();
    FlushFutures();

    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPURequestAdapterInfoStatus_Success, NotNull(), nullptr))
            .WillOnce(WithArg<1>(Invoke([&](const WGPUAdapterInfo* adapterInfo) {
                EXPECT_STREQ(adapterInfo->vendor, fakeAdapterInfo.vendor);
                EXPECT_STREQ(adapterInfo->architecture, fakeAdapterInfo.architecture);
                EXPECT_STREQ(adapterInfo->device, fakeAdapterInfo.device);
                EXPECT_STREQ(adapterInfo->description, fakeAdapterInfo.description);
            })));

        FlushCallbacks();
    });
}

// Test that RequestAdapterInfo receives unknown status if the wire is disconnected before the
// callback happens.
TEST_P(WireAdapterInfoTests, RequestAdapterInfoWireDisconnectedBeforeCallback) {
    AdapterRequestAdapterInfo(adapter);

    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPURequestAdapterInfoStatus_InstanceDropped, _, nullptr))
            .Times(1);

        GetWireClient()->Disconnect();
    });
}

}  // anonymous namespace
}  // namespace dawn::wire
