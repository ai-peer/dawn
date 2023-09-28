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

using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::MockCallback;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;
using testing::WithArg;

class WireInstanceBasicTest : public WireTest {};

using WireInstanceTestBase = WireFutureTestWithParams<WGPURequestAdapterCallback,
                                                      WGPURequestAdapterCallbackInfo,
                                                      wgpuInstanceRequestAdapter,
                                                      wgpuInstanceRequestAdapterF>;
class WireInstanceTests : public WireInstanceTestBase {
  public:
    // Overriden version of wgpuInstanceRequestAdapter that defers to the API call based on the
    // test callback mode.
    void InstanceRequestAdapter(WGPUInstance i,
                                WGPURequestAdapterOptions const* options,
                                WGPURequestAdapterCallback cb,
                                void* userdata) {
        CallImpl(cb, userdata, i, options);
    }

  protected:
    void SetUp() override { WireInstanceTestBase::SetUp(); }

    void TearDown() override { WireInstanceTestBase::TearDown(); }

    NiceMock<MockCallback<WGPURequestAdapterCallback>> mockCallback;
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireInstanceTests);

// Test that an Instance can be reserved and injected into the wire.
TEST_F(WireInstanceBasicTest, ReserveAndInject) {
    auto reservation = GetWireClient()->ReserveInstance();
    wgpu::Instance instance = wgpu::Instance::Acquire(reservation.instance);

    WGPUInstance apiInstance = api.GetNewInstance();
    EXPECT_CALL(api, InstanceReference(apiInstance));
    EXPECT_TRUE(
        GetWireServer()->InjectInstance(apiInstance, reservation.id, reservation.generation));

    instance = nullptr;

    EXPECT_CALL(api, InstanceRelease(apiInstance));
    FlushClient();
}

// Test that RequestAdapterOptions are passed from the client to the server.
TEST_P(WireInstanceTests, RequestAdapterPassesOptions) {
    auto* userdata = mockCallback.MakeUserdata(this);

    for (WGPUPowerPreference powerPreference :
         {WGPUPowerPreference_LowPower, WGPUPowerPreference_HighPerformance}) {
        WGPURequestAdapterOptions options = {};
        options.powerPreference = powerPreference;

        InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

        EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
            .WillOnce(WithArg<1>(Invoke([&](const WGPURequestAdapterOptions* apiOptions) {
                EXPECT_EQ(apiOptions->powerPreference,
                          static_cast<WGPUPowerPreference>(options.powerPreference));
                EXPECT_EQ(apiOptions->forceFallbackAdapter, options.forceFallbackAdapter);
            })));
        FlushClient();
    }

    // Delete the instance now, or it'll call the mock callback after it's deleted.
    instance = nullptr;
}

// Test that RequestAdapter forwards the adapter information to the client.
TEST_P(WireInstanceTests, RequestAdapterSuccess) {
    WGPURequestAdapterOptions options = {};
    auto* userdata = mockCallback.MakeUserdata(this);
    InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

    WGPUAdapterProperties fakeProperties = {};
    fakeProperties.vendorID = 0x134;
    fakeProperties.vendorName = "fake-vendor";
    fakeProperties.architecture = "fake-architecture";
    fakeProperties.deviceID = 0x918;
    fakeProperties.name = "fake adapter";
    fakeProperties.driverDescription = "hello world";
    fakeProperties.backendType = WGPUBackendType_D3D12;
    fakeProperties.adapterType = WGPUAdapterType_IntegratedGPU;

    WGPUSupportedLimits fakeLimits = {};
    fakeLimits.nextInChain = nullptr;
    fakeLimits.limits.maxTextureDimension1D = 433;
    fakeLimits.limits.maxVertexAttributes = 1243;

    std::initializer_list<WGPUFeatureName> fakeFeatures = {
        WGPUFeatureName_Depth32FloatStencil8,
        WGPUFeatureName_TextureCompressionBC,
    };

    // Expect the server to receive the message. Then, mock a fake reply.
    WGPUAdapter apiAdapter = api.GetNewAdapter();
    EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            EXPECT_CALL(api, AdapterGetProperties(apiAdapter, NotNull()))
                .WillOnce(SetArgPointee<1>(fakeProperties));

            EXPECT_CALL(api, AdapterGetLimits(apiAdapter, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                    *limits = fakeLimits;
                    return true;
                })));

            EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, nullptr))
                .WillOnce(Return(fakeFeatures.size()));

            EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUFeatureName* features) {
                    for (WGPUFeatureName feature : fakeFeatures) {
                        *(features++) = feature;
                    }
                    return fakeFeatures.size();
                })));
            api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Success,
                                                   apiAdapter, nullptr);
        }));
    FlushClientFutures();

    // Expect the callback in the client and all the adapter information to match.
    EXPECT_CALL(mockCallback, Call(WGPURequestAdapterStatus_Success, NotNull(), nullptr, this))
        .WillOnce(WithArg<1>(Invoke([&](WGPUAdapter adapter) {
            WGPUAdapterProperties properties = {};
            wgpuAdapterGetProperties(adapter, &properties);
            EXPECT_EQ(properties.vendorID, fakeProperties.vendorID);
            EXPECT_STREQ(properties.vendorName, fakeProperties.vendorName);
            EXPECT_STREQ(properties.architecture, fakeProperties.architecture);
            EXPECT_EQ(properties.deviceID, fakeProperties.deviceID);
            EXPECT_STREQ(properties.name, fakeProperties.name);
            EXPECT_STREQ(properties.driverDescription, fakeProperties.driverDescription);
            EXPECT_EQ(properties.backendType, fakeProperties.backendType);
            EXPECT_EQ(properties.adapterType, fakeProperties.adapterType);

            WGPUSupportedLimits limits = {};
            EXPECT_TRUE(wgpuAdapterGetLimits(adapter, &limits));
            EXPECT_EQ(limits.limits.maxTextureDimension1D, fakeLimits.limits.maxTextureDimension1D);
            EXPECT_EQ(limits.limits.maxVertexAttributes, fakeLimits.limits.maxVertexAttributes);

            std::vector<WGPUFeatureName> features;
            features.resize(wgpuAdapterEnumerateFeatures(adapter, nullptr));
            ASSERT_EQ(features.size(), fakeFeatures.size());
            EXPECT_EQ(wgpuAdapterEnumerateFeatures(adapter, &features[0]), features.size());

            std::unordered_set<WGPUFeatureName> featureSet(fakeFeatures);
            for (WGPUFeatureName feature : features) {
                EXPECT_EQ(featureSet.erase(feature), 1u);
            }
        })));
    FlushServerFutures();
}

// Test that features returned by the implementation that aren't supported in the wire are not
// exposed.
TEST_P(WireInstanceTests, RequestAdapterWireLacksFeatureSupport) {
    WGPURequestAdapterOptions options = {};
    auto* userdata = mockCallback.MakeUserdata(this);
    InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

    std::initializer_list<WGPUFeatureName> fakeFeatures = {
        WGPUFeatureName_Depth32FloatStencil8,
        // Some value that is not a valid feature
        static_cast<WGPUFeatureName>(-2),
    };

    // Expect the server to receive the message. Then, mock a fake reply.
    WGPUAdapter apiAdapter = api.GetNewAdapter();
    EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
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
                .WillOnce(Return(fakeFeatures.size()));

            EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUFeatureName* features) {
                    for (WGPUFeatureName feature : fakeFeatures) {
                        *(features++) = feature;
                    }
                    return fakeFeatures.size();
                })));
            api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Success,
                                                   apiAdapter, nullptr);
        }));
    FlushClientFutures();

    // Expect the callback in the client and all the adapter information to match.
    EXPECT_CALL(mockCallback, Call(WGPURequestAdapterStatus_Success, NotNull(), nullptr, this))
        .WillOnce(WithArg<1>(Invoke([&](WGPUAdapter adapter) {
            WGPUFeatureName feature;
            ASSERT_EQ(wgpuAdapterEnumerateFeatures(adapter, nullptr), 1u);
            wgpuAdapterEnumerateFeatures(adapter, &feature);
            EXPECT_EQ(feature, WGPUFeatureName_Depth32FloatStencil8);
        })));
    FlushServerFutures();
}

// Test that RequestAdapter errors forward to the client.
TEST_P(WireInstanceTests, RequestAdapterError) {
    WGPURequestAdapterOptions options = {};
    auto* userdata = mockCallback.MakeUserdata(this);
    InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

    // Expect the server to receive the message. Then, mock an error.
    EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Error,
                                                   nullptr, "Some error");
        }));
    FlushClientFutures();

    // Expect the callback in the client.
    EXPECT_CALL(mockCallback,
                Call(WGPURequestAdapterStatus_Error, nullptr, StrEq("Some error"), this))
        .Times(1);
    FlushServerFutures();
}

// Test that RequestAdapter receives unknown status if the instance is deleted
// before the callback happens.
TEST_P(WireInstanceTests, RequestAdapterInstanceDestroyedBeforeCallback) {
    WGPURequestAdapterOptions options = {};
    auto* userdata = mockCallback.MakeUserdata(this);
    InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

    // TODO(crbug.com/dawn/2061) This test currently passes, but IIUC, the callback isn't actually
    // triggered by the destruction of the instance at the moment. Instead, the callback happens
    // because we eventually teardown the test. Once we move the EventManager to be per-Instance,
    // this test needs to be updated to verify the mock callback immediately after the destruction
    // of the Instance.
    EXPECT_CALL(mockCallback, Call(WGPURequestAdapterStatus_Unknown, nullptr, NotNull(), this))
        .Times(1);
    wgpuInstanceRelease(instance);
}

// Test that RequestAdapter receives unknown status if the wire is disconnected
// before the callback happens.
TEST_P(WireInstanceTests, RequestAdapterWireDisconnectBeforeCallback) {
    WGPURequestAdapterOptions options = {};
    auto* userdata = mockCallback.MakeUserdata(this);
    InstanceRequestAdapter(instance, &options, mockCallback.Callback(), userdata);

    EXPECT_CALL(mockCallback, Call(WGPURequestAdapterStatus_Unknown, nullptr, NotNull(), this))
        .Times(1);
    GetWireClient()->Disconnect();
}

}  // anonymous namespace
}  // namespace dawn::wire
