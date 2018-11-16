// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_FENCETRACKERD3D12_H_
#define DAWNNATIVE_D3D12_FENCETRACKERD3D12_H_

#include "common/SerialQueue.h"
#include "dawn_native/RefCounted.h"

namespace dawn_native { namespace d3d12 {

    class Device;
    class Fence;

    class FenceTracker {
        struct FenceInFlight {
            Ref<Fence> fence;
            uint64_t value;
        };

      public:
        FenceTracker(Device* device);
        ~FenceTracker();

        void UpdateFenceOnComplete(Fence* fence, uint64_t value);

        void Tick(Serial finishedSerial);

      private:
        Device* mDevice;
        SerialQueue<FenceInFlight> mFencesInFlight;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_FENCETRACKERD3D12_H_
