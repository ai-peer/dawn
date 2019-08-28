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

#ifndef DAWNNATIVE_FENCEDREFTRACKER_H_
#define DAWNNATIVE_FENCEDREFTRACKER_H_

#include "common/SerialQueue.h"
#include "dawn_native/RefCounted.h"

namespace dawn_native {

    class DeviceBase;
    class RefCounted;

    // This class helps hold references to objects until the serial passes on
    // the GPU. It is used to keep objects alive while GPU commands are executed.
    class FencedRefTracker {
      public:
        FencedRefTracker(DeviceBase* device);
        virtual ~FencedRefTracker();

        void ReferenceUntilLastSubmitComplete(RefCounted* obj);

        void Tick(Serial completedSerial);

      protected:
        DeviceBase* mDevice;
        SerialQueue<RefCounted*> mObjectsInFlight;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_FENCEDREFTRACKER_H_
