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

#include "dawn_native/FencedRefTracker.h"

#include "dawn_native/Device.h"
#include "dawn_native/RefCounted.h"

namespace dawn_native {

    FencedRefTracker::FencedRefTracker(DeviceBase* device) : mDevice(device) {
    }

    FencedRefTracker::~FencedRefTracker() {
        ASSERT(mObjectsInFlight.Empty());
    }

    void FencedRefTracker::ReferenceUntilPendingSubmitComplete(RefCounted* obj) {
        obj->Reference();
        mObjectsInFlight.Enqueue(obj, mDevice->GetPendingCommandSerial());
    }

    void FencedRefTracker::Tick(Serial completedSerial) {
        for (RefCounted* obj : mObjectsInFlight.IterateUpTo(completedSerial)) {
            obj->Release();
        }
        mObjectsInFlight.ClearUpTo(completedSerial);
    }

}  // namespace dawn_native
