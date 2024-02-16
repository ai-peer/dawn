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

#ifndef SRC_DAWN_NATIVE_VULKAN_SWAPCHAINCACHE_H_
#define SRC_DAWN_NATIVE_VULKAN_SWAPCHAINCACHE_H_

#include <mutex>
#include <optional>
#include <queue>

#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn::native::vulkan {

class Device;

// After unconfiguring a surface, we keep its swap chain in this cache, together with the serial at
// which it expires. If the previous swap chain still hasn't been destroyed upon re-Configure-ing
// the surface, we use it as the previous swap chain.
// TODO(dawn:2320) If the Surface would have backend-specific subclasses, we could move this
// mechanism to dawn::native::vulkan::Surface and remove SwapChainCache altogether
class SwapChainCache {
  public:
    explicit SwapChainCache(Device* device);

    // Store a swap chain and its expiration serial in this cache
    // This swap chain may be reused as "previous swap chain" by createSwapChain as long as the
    // expiration serial has not been completed yet.
    void RecycleSwapChain(VkSwapchainKHR swapChain,
                          ExecutionSerial expirationSerial,
                          Surface* surface);

    // Get a swap chain that is already queued in the GetFencedDeleter but that has not been
    // destroyed yet. This removes the swap chain from the cache.
    // If no matching cache entry was found, this returns VK_NULL_HANDLE
    // TODO(dawn:1662) We need to lock the fenced deleter, as if someone does something on another
    // thread that makes Dawn flush the deleter, it could race and we end up using a deleted swap
    // chain. Probably what we should ultimately do is actually steal the object out of the fenced
    // deleter entirely. That way it has a single owner, and there's no chance of it getting deleted
    // from under us.
    VkSwapchainKHR AcquireRecycledSwapChain(Surface* surface);

  private:
    raw_ptr<Device> mDevice = nullptr;

    std::mutex mMutex;

    // TODO(dawn:2320) Use a less naive cache structure.
    struct Entry {
        VkSwapchainKHR swapChain;
        ExecutionSerial expirationSerial;
        Surface* surface;  // cache key
    };
    std::queue<Entry> mCache;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_SWAPCHAINCACHE_H_
