// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/vulkan/SwapChainCache.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/QueueVk.h"

namespace dawn::native::vulkan {

SwapChainCache::SwapChainCache(Device* device) : mDevice(device) {}

void SwapChainCache::RecycleSwapChain(VkSwapchainKHR swapChain,
                                      ExecutionSerial expirationSerial,
                                      Surface* surface) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCache.push({swapChain, expirationSerial, surface});
}

std::optional<VkSwapchainKHR> SwapChainCache::AcquireRecycledSwapChain(Surface* surface) {
    std::lock_guard<std::mutex> lock(mMutex);
    ExecutionSerial completedSerial = mDevice->GetQueue()->GetCompletedCommandSerial();
    for (size_t remaining = mCache.size(); remaining > 0; --remaining) {
        Entry entry = mCache.front();
        mCache.pop();
        if (entry.expirationSerial > completedSerial) {
            if (surface == entry.surface) {
                return entry.swapChain;
            } else {
                // Keep in the cache as long as it is not expired
                mCache.push(entry);
            }
        }
    }
    return {};
}

}  // namespace dawn::native::vulkan
