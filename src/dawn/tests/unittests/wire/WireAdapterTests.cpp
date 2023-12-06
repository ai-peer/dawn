// Copyright 2021 The Dawn & Tint Authors
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

#include <unordered_set>
#include <vector>

#include "dawn/tests/MockCallback.h"
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

class WireAdapterTests : public WireTest {};

// Test that the DeviceDescriptor is passed from the client to the server.
TEST_F(WireAdapterTests, RequestDevicePassesDescriptor) {
    // Test an empty descriptor
    {
        MockCallback<WGPURequestDeviceCallback> cb;
        auto* userdata = cb.MakeUserdata(this);

        WGPUDeviceDescriptor desc = {};
        wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

        WGPUDevice apiDevice2 = api.GetNewDevice();
        EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
            .WillOnce(WithArg<1>(Invoke([&](const WGPUDeviceDescriptor* apiDesc) {
                EXPECT_EQ(apiDesc->label, nullptr);
                EXPECT_EQ(apiDesc->requiredFeatureCount, 0u);
                EXPECT_EQ(apiDesc->requiredLimits, nullptr);

                EXPECT_CALL(api, DeviceGetLimits(apiDevice2, NotNull()))
                    .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                        *limits = {};
                        return true;
                    })));

                EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice2, nullptr))
                    .WillOnce(Return(0))
                    .WillOnce(Return(0));

                api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                     apiDevice2, nullptr);
            })));
        FlushClient();

        WGPUDevice device2;
        EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
            .WillOnce(SaveArg<1>(&device2));
        FlushServer();
    }

    // Test a non-empty descriptor
    {
        MockCallback<WGPURequestDeviceCallback> cb;
        auto* userdata = cb.MakeUserdata(this);

        WGPURequiredLimits limits = {};
        limits.limits.maxStorageTexturesPerShaderStage = 5;

        std::vector<WGPUFeatureName> features = {WGPUFeatureName_TextureCompressionETC2,
                                                 WGPUFeatureName_TextureCompressionASTC};

        WGPUDeviceDescriptor desc = {};
        desc.label = "hello device";
        desc.requiredLimits = &limits;
        desc.requiredFeatureCount = features.size();
        desc.requiredFeatures = features.data();

        wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

        WGPUDevice apiDevice2 = api.GetNewDevice();
        EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
            .WillOnce(WithArg<1>(Invoke([&](const WGPUDeviceDescriptor* apiDesc) {
                EXPECT_STREQ(apiDesc->label, desc.label);

                ASSERT_EQ(apiDesc->requiredFeatureCount, features.size());
                for (uint32_t i = 0; i < features.size(); ++i) {
                    EXPECT_EQ(apiDesc->requiredFeatures[i],
                              static_cast<WGPUFeatureName>(features[i]));
                }

                ASSERT_NE(apiDesc->requiredLimits, nullptr);
                EXPECT_EQ(apiDesc->requiredLimits->nextInChain, nullptr);
                EXPECT_EQ(apiDesc->requiredLimits->limits.maxStorageTexturesPerShaderStage,
                          limits.limits.maxStorageTexturesPerShaderStage);

                EXPECT_CALL(api, DeviceGetLimits(apiDevice2, NotNull()))
                    .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                        limits->limits = desc.requiredLimits->limits;
                        return true;
                    })));

                EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice2, nullptr))
                    .WillOnce(Return(features.size()));

                EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice2, NotNull()))
                    .WillOnce(WithArg<1>(Invoke([&](WGPUFeatureName* featuresOut) {
                        for (WGPUFeatureName feature : features) {
                            *(featuresOut++) = feature;
                        }
                        return features.size();
                    })));

                api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                     apiDevice2, nullptr);
            })));
        FlushClient();

        WGPUDevice device2;
        EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
            .WillOnce(SaveArg<1>(&device2));
        FlushServer();
    }
}

// Test that RequestDevice forwards the device information to the client.
TEST_F(WireAdapterTests, RequestDeviceSuccess) {
    MockCallback<WGPURequestDeviceCallback> cb;
    auto* userdata = cb.MakeUserdata(this);

    wgpu::SupportedLimits fakeLimits = {};
    fakeLimits.limits.maxTextureDimension1D = 433;
    fakeLimits.limits.maxVertexAttributes = 1243;

    std::initializer_list<wgpu::FeatureName> fakeFeatures = {
        wgpu::FeatureName::Depth32FloatStencil8,
        wgpu::FeatureName::TextureCompressionBC,
    };

    WGPUDeviceDescriptor desc = {};
    wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

    // Expect the server to receive the message. Then, mock a fake reply.
    WGPUDevice apiDevice = api.GetNewDevice();
    // The backend device should not be known by the wire server.
    EXPECT_FALSE(GetWireServer()->IsDeviceKnown(apiDevice));

    wgpu::Device device;
    EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
        .WillOnce(Invoke(WithArg<1>([&](const auto* deviceDesc) {
            DawnProcTable mockProcs;
            api.GetProcTable(&mockProcs);

            // Forward the callbacks to the mock callback storage.
            EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, _, _));
            EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(apiDevice, _, _));
            mockProcs.deviceSetUncapturedErrorCallback(apiDevice,
                                                       deviceDesc->uncapturedErrorCallback,
                                                       deviceDesc->uncapturedErrorUserdata);
            mockProcs.deviceSetDeviceLostCallback(apiDevice, deviceDesc->deviceLostCallback,
                                                  deviceDesc->deviceLostUserdata);

            EXPECT_CALL(api, DeviceGetLimits(apiDevice, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                    *reinterpret_cast<wgpu::SupportedLimits*>(limits) = fakeLimits;
                    return true;
                })));

            EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice, nullptr))
                .WillOnce(Return(fakeFeatures.size()));

            EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUFeatureName* features) {
                    for (wgpu::FeatureName feature : fakeFeatures) {
                        *(features++) = static_cast<WGPUFeatureName>(feature);
                    }
                    return fakeFeatures.size();
                })));

            // The backend device should still not be known by the wire server since the
            // callback has not been called yet.
            EXPECT_FALSE(GetWireServer()->IsDeviceKnown(apiDevice));
            api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                 apiDevice, nullptr);
            // After the callback is called, the backend device is now known by the server.
            EXPECT_TRUE(GetWireServer()->IsDeviceKnown(apiDevice));
        })));
    FlushClient();

    // Expect the callback in the client and all the device information to match.
    EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
        .WillOnce(WithArg<1>(Invoke([&](WGPUDevice cDevice) {
            device = wgpu::Device::Acquire(cDevice);

            wgpu::SupportedLimits limits;
            EXPECT_TRUE(device.GetLimits(&limits));
            EXPECT_EQ(limits.limits.maxTextureDimension1D, fakeLimits.limits.maxTextureDimension1D);
            EXPECT_EQ(limits.limits.maxVertexAttributes, fakeLimits.limits.maxVertexAttributes);

            std::vector<wgpu::FeatureName> features;
            features.resize(device.EnumerateFeatures(nullptr));
            ASSERT_EQ(features.size(), fakeFeatures.size());
            EXPECT_EQ(device.EnumerateFeatures(&features[0]), features.size());

            std::unordered_set<wgpu::FeatureName> featureSet(fakeFeatures);
            for (wgpu::FeatureName feature : features) {
                EXPECT_EQ(featureSet.erase(feature), 1u);
            }
        })));
    FlushServer();

    // Test that callbacks can propagate from server to client.
    MockCallback<WGPUErrorCallback> errorCb;
    device.SetUncapturedErrorCallback(errorCb.Callback(), errorCb.MakeUserdata(this));
    api.CallDeviceSetUncapturedErrorCallbackCallback(apiDevice, WGPUErrorType_Validation,
                                                     "Some error message");

    EXPECT_CALL(errorCb, Call(WGPUErrorType_Validation, StrEq("Some error message"), this))
        .Times(1);
    FlushServer();

    device = nullptr;
    // Cleared when the device is destroyed.
    EXPECT_CALL(api, DeviceRelease(apiDevice));

    // Server has not recevied the release yet, so the device should be known.
    EXPECT_TRUE(GetWireServer()->IsDeviceKnown(apiDevice));
    FlushClient();
    // After receiving the release call, the device is no longer known by the server.
    EXPECT_FALSE(GetWireServer()->IsDeviceKnown(apiDevice));
}

// Test that features requested that the implementation supports, but not the
// wire reject the callback.
TEST_F(WireAdapterTests, RequestFeatureUnsupportedByWire) {
    MockCallback<WGPURequestDeviceCallback> cb;
    auto* userdata = cb.MakeUserdata(this);

    std::initializer_list<wgpu::FeatureName> fakeFeatures = {
        // Some value that is not a valid feature
        static_cast<wgpu::FeatureName>(-2),
        wgpu::FeatureName::TextureCompressionASTC,
    };

    WGPUDeviceDescriptor desc = {};
    wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

    // Expect the server to receive the message. Then, mock a fake reply.
    // The reply contains features that the device implementation supports, but the
    // wire does not.
    WGPUDevice apiDevice = api.GetNewDevice();
    EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice, nullptr))
                .WillOnce(Return(fakeFeatures.size()));

            EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUFeatureName* features) {
                    for (wgpu::FeatureName feature : fakeFeatures) {
                        *(features++) = static_cast<WGPUFeatureName>(feature);
                    }
                    return fakeFeatures.size();
                })));

            // The device was actually created, but the wire didn't support its features.
            // Expect it to be released.
            EXPECT_CALL(api, DeviceRelease(apiDevice));

            // Fake successful creation. The client still receives a failure due to
            // unsupported features.
            api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                 apiDevice, nullptr);
        }));
    FlushClient();

    // Expect an error callback since the feature is not supported.
    EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Error, nullptr, NotNull(), this)).Times(1);
    FlushServer();
}

// Test that RequestDevice errors forward to the client.
TEST_F(WireAdapterTests, RequestDeviceError) {
    MockCallback<WGPURequestDeviceCallback> cb;
    auto* userdata = cb.MakeUserdata(this);

    WGPUDeviceDescriptor desc = {};
    wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

    // Expect the server to receive the message. Then, mock an error.
    EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Error, nullptr,
                                                 "Request device failed");
        }));
    FlushClient();

    // Expect the callback in the client.
    EXPECT_CALL(cb,
                Call(WGPURequestDeviceStatus_Error, nullptr, StrEq("Request device failed"), this))
        .Times(1);
    FlushServer();
}

// Test that RequestDevice receives unknown status if the adapter is deleted
// before the callback happens.
TEST_F(WireAdapterTests, RequestDeviceAdapterDestroyedBeforeCallback) {
    MockCallback<WGPURequestDeviceCallback> cb;
    auto* userdata = cb.MakeUserdata(this);

    WGPUDeviceDescriptor desc = {};
    wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

    EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Unknown, nullptr, NotNull(), this)).Times(1);
    wgpuAdapterRelease(adapter);
}

// Test that RequestDevice receives unknown status if the wire is disconnected
// before the callback happens.
TEST_F(WireAdapterTests, RequestDeviceWireDisconnectedBeforeCallback) {
    MockCallback<WGPURequestDeviceCallback> cb;
    auto* userdata = cb.MakeUserdata(this);

    WGPUDeviceDescriptor desc = {};
    wgpuAdapterRequestDevice(adapter, &desc, cb.Callback(), userdata);

    EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Unknown, nullptr, NotNull(), this)).Times(1);
    GetWireClient()->Disconnect();
}

}  // anonymous namespace
}  // namespace dawn::wire
