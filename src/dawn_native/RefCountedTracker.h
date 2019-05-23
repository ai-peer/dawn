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

#ifndef DAWNNATIVE_REFCOUNTEDTRACKER_H_
#define DAWNNATIVE_REFCOUNTEDTRACKER_H_

#include "common/SerialQueue.h"
#include "dawn_native/RefCounted.h"

namespace dawn_native {

    class DeviceBase;

    class RefCountedTracker {
      public:
        RefCountedTracker(DeviceBase* device);
        ~RefCountedTracker() = default;

        void Track(RefCounted* object);
        void Tick(Serial finishedSerial);

      private:
        DeviceBase* mDevice;
        SerialQueue<Ref<RefCounted>> mRefsInFlight;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_REFCOUNTEDTRACKER_H_
