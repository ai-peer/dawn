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

#ifndef SRC_DAWN_NATIVE_DEVICECYCLEBREAKINGREFCOUNTED_H_
#define SRC_DAWN_NATIVE_DEVICECYCLEBREAKINGREFCOUNTED_H_

#include "dawn/common/Assert.h"
#include "dawn/common/RefCounted.h"

namespace dawn::native {

// DeviceCycleBreakingRefCounted is a version of RefCounted
// used by DeviceBase to break refcycles. DeviceBase holds
// multiple Refs to various API objects (pipelines, buffers, etc.)
// which are used to implement various device-level facilities.
// These objects are cached on the device, so we want to keep them
// around instead of making transient allocations.
// However, the objects also hold a Ref<Device> back to their parent
// device.
//    In order to break this cycle and prevent leaks,
// DeviceCycleBreakingRefCounted keeps track of the number of external
// refs that the application is holding. This is tracked by counting
// calls to APIReference/APIRelease. There is also external ref that
// is added by calling Externalize(). This must be done right before
// Dawn returns the device to the application in APIRequestDevice
// and APICreateDevice.
//    Just be for the last external reference is released, WillDropLastExternalRef
// is called. Here, the device can clear out any member Refs to API objects
// that are hold back-refs to the device - thus breaking any reference
// cycles.
class DeviceCycleBreakingRefCounted : private RefCounted {
  public:
    using RefCounted::RefCounted;
    using RefCounted::Reference;
    using RefCounted::Release;

    void Externalize();
    void APIReference();
    void APIRelease();

  private:
    virtual void WillDropLastExternalRef() = 0;

    std::atomic<uint64_t> mExternalRefCount{0};
#if defined(DAWN_ENABLE_ASSERTS)
    bool mExternalized = false;
#endif
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_DEVICECYCLEBREAKINGREFCOUNTED_H_
