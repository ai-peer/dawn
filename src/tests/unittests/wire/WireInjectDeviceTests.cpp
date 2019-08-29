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

#include "tests/unittests/wire/WireTest.h"

#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"

using namespace testing;
using namespace dawn_wire;

class WireInjectDeviceTests : public WireTest {
  public:
    WireInjectDeviceTests() {
    }
    ~WireInjectDeviceTests() override = default;
};

// Test that reserve correctly returns different IDs each time.
TEST_F(WireInjectDeviceTests, ReserveDifferentIDs) {
    ReservedDevice reservation1 = GetWireClient()->ReserveDevice();
    ReservedDevice reservation2 = GetWireClient()->ReserveDevice();

    ASSERT_NE(reservation1.id, reservation2.id);
    ASSERT_NE(reservation1.device, reservation2.device);
}

// Test that injecting the same id without a destroy first fails.
TEST_F(WireInjectDeviceTests, InjectExistingID) {
    ReservedDevice reservation = GetWireClient()->ReserveDevice();

    // Injecting the device adds a reference
    DawnDevice apiDevice = api.GetNewDevice();
    EXPECT_CALL(api, DeviceReference(apiDevice));
    EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, _, _)).Times(1);
    ASSERT_TRUE(GetWireServer()->InjectDevice(apiDevice, reservation.id, reservation.generation));

    // ID already in use, call fails.
    ASSERT_FALSE(GetWireServer()->InjectDevice(apiDevice, reservation.id, reservation.generation));
}

// Test that the server only borrows the device and does a single reference-release
TEST_F(WireInjectDeviceTests, InjectedDeviceLifetime) {
    ReservedDevice reservation = GetWireClient()->ReserveDevice();

    // Injecting the device adds a reference
    DawnDevice apiDevice = api.GetNewDevice();
    EXPECT_CALL(api, DeviceReference(apiDevice));
    EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, _, _)).Times(1);
    ASSERT_TRUE(GetWireServer()->InjectDevice(apiDevice, reservation.id, reservation.generation));

    // Releasing the device removes a single reference.
    dawnDeviceRelease(reservation.device);
    EXPECT_CALL(api, DeviceRelease(apiDevice));
    FlushClient();

    // Deleting the server doesn't release a second reference.
    DeleteServer();
    Mock::VerifyAndClearExpectations(&api);
}
