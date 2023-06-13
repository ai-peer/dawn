// Copyright 2023 The Dawn Authors
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

#include "dawn/native/EventManager.h"

#include <vector>

#include "dawn/common/FutureUtils.h"
#include "dawn/native/Device.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/OSEventReceiver.h"

namespace dawn::native {

namespace {

wgpu::WaitStatus WaitImpl(std::vector<TrackedFutureWaitInfo>& futures, Nanoseconds timeout) {
    // Sort the futures by how they'll be waited (their GetWaitDevice).
    // This lets us do each wait on a slice of the array.
    std::ranges::sort(futures, std::ranges::less{}, [](const TrackedFutureWaitInfo& future) {
        return future.event->GetWaitDevice();
    });

    if (timeout > Nanoseconds(0)) {
        ASSERT(futures.size() <= kTimedWaitAnyMaxCountDefault);

        // If there's a timeout, check that there isn't a mix of wait devices.
        if (futures.front().event->GetWaitDevice() != futures.back().event->GetWaitDevice()) {
            return wgpu::WaitStatus::UnsupportedMixedSources;
        }
    }

    // Actually do the poll or wait to find out if any of the futures became ready.
    // Here, there's either only one iteration, or timeout is 0, so we know the
    // timeout won't get stacked multiple times.
    bool anySuccess = false;
    // Find each slice of the array (sliced by wait device), and wait on it.
    for (size_t sliceStart = 0; sliceStart < futures.size();) {
        DeviceBase* waitDevice = (futures[sliceStart].event->GetWaitDevice());
        size_t sliceLength = 1;
        while (sliceStart + sliceLength < futures.size() &&
               (futures[sliceStart + sliceLength].event->GetWaitDevice()) == waitDevice) {
            sliceLength++;
        }

        {
            bool success;
            if (waitDevice) {
                success = waitDevice->WaitAnyImpl(sliceLength, &futures[sliceStart], timeout);
            } else {
                success = OSEventReceiver::Wait(sliceLength, &futures[sliceStart], timeout);
            }
            anySuccess |= success;
        }

        sliceStart += sliceLength;
    }
    if (!anySuccess) {
        return wgpu::WaitStatus::TimedOut;
    }
    return wgpu::WaitStatus::Success;
}

}  // namespace

MaybeError EventManager::Initialize(const InstanceDescriptor* descriptor) {
    if (descriptor) {
        if (descriptor->timedWaitAnyMaxCount > kTimedWaitAnyMaxCountDefault) {
            return DAWN_VALIDATION_ERROR("Requested timedWaitAnyMaxCount is not supported");
        }
        mTimedWaitEnable = descriptor->timedWaitAnyEnable;
    }

    return {};
}

FutureID EventManager::Track(WGPUCallbackModeFlags mode, TrackedEvent* future) {
    bool isFuture = mode & WGPUCallbackMode_Future;
    bool isProcessEvents = mode & WGPUCallbackMode_ProcessEvents;
    ASSERT(!(isFuture && isProcessEvents));

    if (isFuture) {
        std::lock_guard lock{mTrackedFuturesMutex};

        FutureID futureID = mNextFutureID++;
        // mNextFutureID must be incremented inside the lock. Otherwise, there's a gap (where this
        // comment is) during which WaitAny will incorrectly assume this future is Completed.
        mTrackedFutures.emplace(futureID, future);

        return futureID;
    } else if (isProcessEvents) {
        std::lock_guard lock{mTrackedPollEventsMutex};

        FutureID futureID = mNextFutureID++;
        mTrackedPollEvents.emplace(futureID, future);

        // Return 0 (null future ID), because the user didn't actually ask for a future.
        return 0;
    } else {
        ASSERT(mode & WGPUCallbackMode_Spontaneous);
        return 0;
    }
}

void EventManager::ProcessPollEvents() {
    std::vector<TrackedFutureWaitInfo> futures;
    futures.reserve(mTrackedPollEvents.size());

    {
        std::lock_guard lock{mTrackedPollEventsMutex};
        for (auto& [futureID, event] : mTrackedPollEvents) {
            // FIXME: This WaitRef is unnecessary, because we have an instance-wide lock.
            // We also don't need some of the other stuff.
            futures.push_back(TrackedFutureWaitInfo{futureID, event->TakeWaitRef(), 0, false});
        }

        // The WaitImpl is inside of the lock to prevent any two ProcessEvents calls from
        // calling competing OS wait syscalls at the same time.
        wgpu::WaitStatus waitStatus = WaitImpl(futures, Nanoseconds(0));
        if (waitStatus == wgpu::WaitStatus::TimedOut) {
            return;
        }
        ASSERT(waitStatus == wgpu::WaitStatus::Success);

        for (TrackedFutureWaitInfo& future : futures) {
            if (future.ready) {
                mTrackedPollEvents.erase(future.futureID);
            }
        }
    }

    for (TrackedFutureWaitInfo& future : futures) {
        if (future.ready) {
            future.event->EnsureCompleteFromProcessEvents();
        }
    }
}

wgpu::WaitStatus EventManager::WaitAny(size_t count, FutureWaitInfo* infos, Nanoseconds timeout) {
    if (count == 0) {
        return wgpu::WaitStatus::Success;
    }

    // Look up all of the futures and build a list of `TrackedFutureWaitInfo`s.
    std::vector<TrackedFutureWaitInfo> futures;
    futures.reserve(count);
    bool anyCompleted = false;
    {
        std::lock_guard lock{mTrackedFuturesMutex};

        FutureID firstInvalidFutureID = mNextFutureID;
        for (size_t i = 0; i < count; ++i) {
            FutureID futureID = infos[i].future.id;

            // Check for cases that are undefined behavior in the API contract.
            ASSERT(futureID != 0);
            ASSERT(futureID < firstInvalidFutureID);
            // TakeWaitRef below will catch if the future is waited twice at the
            // same time (unless it's already completed).

            auto it = mTrackedFutures.find(futureID);
            if (it == mTrackedFutures.end()) {
                infos[i].completed = true;
                anyCompleted = true;
            } else {
                infos[i].completed = false;
                futures.push_back(
                    TrackedFutureWaitInfo{futureID, it->second->TakeWaitRef(), i, false});
            }
        }
    }
    // If any completed, return immediately.
    if (anyCompleted) {
        return wgpu::WaitStatus::Success;
    }
    // Otherwise, we should have successfully looked up all of them.
    ASSERT(futures.size() == count);

    // Validate for feature support.
    // Note this is after .completed fields get set, so they'll be correct even if there's an error.
    if (timeout > Nanoseconds(0)) {
        if (!mTimedWaitEnable) {
            return wgpu::WaitStatus::UnsupportedTimeout;
        }
        if (count > mTimedWaitMaxCount) {
            return wgpu::WaitStatus::UnsupportedCount;
        }
    }

    wgpu::WaitStatus waitStatus = WaitImpl(futures, timeout);
    if (waitStatus != wgpu::WaitStatus::Success) {
        return waitStatus;
    }

    // For any futures that we're about to complete, first ensure they're untracked. It's OK if
    // something actually isn't tracked anymore (because it completed elsewhere while waiting.)
    {
        std::lock_guard lock{mTrackedFuturesMutex};
        for (const TrackedFutureWaitInfo& future : futures) {
            if (future.ready) {
                mTrackedFutures.erase(future.futureID);
            }
        }
    }

    // Finally, call callbacks and update return values.
    for (TrackedFutureWaitInfo& future : futures) {
        if (future.ready) {
            // TODO(crbug.com/dawn/1987): Guarantee the event ordering from the JS spec.
            future.event->EnsureCompleteFromWaitAny();
            infos[future.indexInInfos].completed = true;
        }
    }

    return wgpu::WaitStatus::Success;
}

}  // namespace dawn::native
