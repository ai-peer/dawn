// Copyright 2019 The Dawn & Tint Authors
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

#include "dawn/wire/server/Server.h"
#include "dawn/wire/WireServer.h"

namespace dawn::wire::server {

CallbackUserdata::CallbackUserdata(Server* server, const std::shared_ptr<bool>& serverIsAlive)
    : server(server), serverIsAlive(serverIsAlive) {}

Server::Server(const DawnProcTable& procs,
               CommandSerializer* serializer,
               MemoryTransferService* memoryTransferService)
    : mSerializer(serializer),
      mProcs(procs),
      mMemoryTransferService(memoryTransferService),
      mIsAlive(std::make_shared<bool>(true)) {
    if (mMemoryTransferService == nullptr) {
        // If a MemoryTransferService is not provided, fallback to inline memory.
        mOwnedMemoryTransferService = CreateInlineMemoryTransferService();
        mMemoryTransferService = mOwnedMemoryTransferService.get();
    }
}

Server::~Server() {
    DestroyAllObjects(mProcs);
}

WireResult Server::InjectTexture(WGPUTexture texture,
                                 uint32_t id,
                                 uint32_t generation,
                                 uint32_t deviceId,
                                 uint32_t deviceGeneration) {
    DAWN_ASSERT(texture != nullptr);
    Known<WGPUDevice> device;
    WIRE_TRY(DeviceObjects().Get(deviceId, &device));
    if (device->generation != deviceGeneration) {
        return WireResult::FatalError;
    }

    Known<WGPUTexture> data;
    WIRE_TRY(TextureObjects().Allocate(&data, ObjectHandle{id, generation}));

    data->handle = texture;
    data->generation = generation;
    data->state = AllocationState::Allocated;

    // The texture is externally owned so it shouldn't be destroyed when we receive a destroy
    // message from the client. Add a reference to counterbalance the eventual release.
    mProcs.textureReference(texture);

    return WireResult::Success;
}

WireResult Server::InjectSwapChain(WGPUSwapChain swapchain,
                                   uint32_t id,
                                   uint32_t generation,
                                   uint32_t deviceId,
                                   uint32_t deviceGeneration) {
    DAWN_ASSERT(swapchain != nullptr);
    Known<WGPUDevice> device;
    WIRE_TRY(DeviceObjects().Get(deviceId, &device));
    if (device->generation != deviceGeneration) {
        return WireResult::FatalError;
    }

    Known<WGPUSwapChain> data;
    WIRE_TRY(SwapChainObjects().Allocate(&data, ObjectHandle{id, generation}));

    data->handle = swapchain;
    data->generation = generation;
    data->state = AllocationState::Allocated;

    // The texture is externally owned so it shouldn't be destroyed when we receive a destroy
    // message from the client. Add a reference to counterbalance the eventual release.
    mProcs.swapChainReference(swapchain);

    return WireResult::Success;
}

WireResult Server::InjectInstance(WGPUInstance instance, uint32_t id, uint32_t generation) {
    DAWN_ASSERT(instance != nullptr);
    Known<WGPUInstance> data;
    WIRE_TRY(InstanceObjects().Allocate(&data, ObjectHandle{id, generation}));

    data->handle = instance;
    data->generation = generation;
    data->state = AllocationState::Allocated;

    // The instance is externally owned so it shouldn't be destroyed when we receive a destroy
    // message from the client. Add a reference to counterbalance the eventual release.
    mProcs.instanceReference(instance);

    return WireResult::Success;
}

WGPUDevice Server::GetDevice(uint32_t id, uint32_t generation) {
    Known<WGPUDevice> device;
    if (DeviceObjects().Get(id, &device) != WireResult::Success ||
        device->generation != generation) {
        return nullptr;
    }
    return device->handle;
}

bool Server::IsDeviceKnown(WGPUDevice device) const {
    return DeviceObjects().IsKnown(device);
}

}  // namespace dawn::wire::server
