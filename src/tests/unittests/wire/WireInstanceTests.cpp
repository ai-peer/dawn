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

#include "tests/MockCallback.h"
#include "tests/unittests/wire/WireTest.h"

#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"

#include <webgpu/webgpu_cpp.h>

using namespace testing;
using namespace dawn_wire;

class WireInstanceTests : public WireTest {
};

// Test that an Instance can be reserved and injected into the wire.
TEST_F(WireInstanceTests, ReserveAndInject) {
  auto reservation = GetWireClient()->ReserveInstance();
  wgpu::Instance instance = wgpu::Instance::Acquire(reservation.instance);

  WGPUInstance apiInstance = api.GetNewInstance();
  EXPECT_CALL(api, InstanceReference(apiInstance));
  EXPECT_TRUE(GetWireServer()->InjectInstance(apiInstance, reservation.id, reservation.generation));

  instance = nullptr;

  EXPECT_CALL(api, InstanceRelease(apiInstance));
  FlushClient();
}

TEST_F(WireInstanceTests, RequestAdapter) {
  auto reservation = GetWireClient()->ReserveInstance();
  wgpu::Instance instance = wgpu::Instance::Acquire(reservation.instance);

  WGPUInstance apiInstance = api.GetNewInstance();
  EXPECT_CALL(api, InstanceReference(apiInstance));
  EXPECT_TRUE(GetWireServer()->InjectInstance(apiInstance, reservation.id, reservation.generation));

  wgpu::RequestAdapterOptions options = {};
  MockCallback<WGPURequestAdapterCallback> cb;
  auto* userdata = cb.MakeUserdata(this);
  instance.RequestAdapter(&options, cb.Callback(), userdata);

  // Expect the server to receive the message. Then, mock a fake reply.
  WGPUAdapter apiAdapter = api.GetNewAdapter();
  EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
    .WillOnce(InvokeWithoutArgs([&]() {
      EXPECT_CALL(api, AdapterGetProperties(apiAdapter, NotNull()));
      EXPECT_CALL(api, AdapterGetLimits(apiAdapter, NotNull()));
      // This is called twice with nullptr because the mock adapter enumeration returns nothing.
      EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, nullptr))
        .Times(2);
      api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Success, apiAdapter, nullptr);
    }));
  FlushClient();

  EXPECT_CALL(cb, Call(WGPURequestAdapterStatus_Success, NotNull(), nullptr, userdata));
  FlushServer();
}
