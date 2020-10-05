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

#include "dawn_native/FenceSignalTracker.h"

#include "dawn_native/Device.h"
#include "dawn_native/Fence.h"

namespace dawn_native {

    FenceSignalTracker::FenceSignalTracker(DeviceBase* device) : mDevice(device) {
    }

    void FenceSignalTracker::UpdateFenceOnComplete(Fence* fence, FenceAPISerial value) {
        std::unique_ptr<FenceInFlight> fenceInFlight = std::make_unique<FenceInFlight>();
        fenceInFlight->fence = fence;
        fenceInFlight->value = value;
        fenceInFlight->type = QueueBase::TaskInFlight::Type::FenceInFlightTask;
        mDevice->GetDefaultQueue()->TrackTasksInFlight(std::move(fenceInFlight));
    }

}  // namespace dawn_native
