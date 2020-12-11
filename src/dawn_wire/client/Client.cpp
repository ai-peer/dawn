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

#include "dawn_wire/client/Client.h"

#include "common/Compiler.h"
#include "dawn_wire/client/Device.h"

namespace dawn_wire { namespace client {

    namespace {

        class NoopCommandSerializer final : public CommandSerializer {
          public:
            static NoopCommandSerializer* GetInstance() {
                static NoopCommandSerializer gNoopCommandSerializer;
                return &gNoopCommandSerializer;
            }

            ~NoopCommandSerializer() = default;

            size_t GetMaximumAllocationSize() const final {
                return 0;
            }
            void* GetCmdSpace(size_t size) final {
                return nullptr;
            }
            bool Flush() final {
                return false;
            }
        };

    }  // anonymous namespace

    Client::Client(CommandSerializer* serializer, MemoryTransferService* memoryTransferService)
        : ClientBase(),
          mInstance(InstanceAllocator().New(this)->object.get()),
          mSerializer(serializer),
          mMemoryTransferService(memoryTransferService) {
        if (mMemoryTransferService == nullptr) {
            // If a MemoryTransferService is not provided, fall back to inline memory.
            mOwnedMemoryTransferService = CreateInlineMemoryTransferService();
            mMemoryTransferService = mOwnedMemoryTransferService.get();
        }
    }

    Client::~Client() {
        DestroyAllObjects();
        InstanceAllocator().Free(mInstance);
        ASSERT(mInstances.empty());
    }

    void Client::DestroyAllObjects() {
        while (!mDevices.empty()) {
            ObjectBase* device = mDevices.head()->value();

            DestroyObjectCmd cmd;
            cmd.objectType = ObjectType::Device;
            cmd.objectId = device->id;
            SerializeCommand(cmd);
            FreeObject(cmd.objectType, device);
        }

        while (!mAdapters.empty()) {
            ObjectBase* adapter = mAdapters.head()->value();

            DestroyObjectCmd cmd;
            cmd.objectType = ObjectType::Adapter;
            cmd.objectId = adapter->id;
            SerializeCommand(cmd);
            FreeObject(cmd.objectType, adapter);
        }
    }

    WGPUInstance Client::GetInstance() {
        return reinterpret_cast<WGPUInstance>(mInstance);
    }

    ReservedTexture Client::ReserveTexture(WGPUDevice cDevice) {
        Device* device = FromAPI(cDevice);
        auto* allocation = TextureAllocator().New(device);

        ReservedTexture result;
        result.texture = ToAPI(allocation->object.get());
        result.id = allocation->object->id;
        result.generation = allocation->generation;
        return result;
    }

    void Client::Disconnect() {
        mDisconnected = true;
        mSerializer = ChunkedCommandSerializer(NoopCommandSerializer::GetInstance());

        LinkNode<ObjectBase>* deviceNode = mDevices.head();
        while (deviceNode != mDevices.end()) {
            Device* device = static_cast<Device*>(deviceNode->value());
            device->HandleDeviceLost("GPU connection lost");
            device->CancelCallbacksForDisconnect();

            deviceNode = deviceNode->next();
        }
    }

    void Client::TrackObject(Instance* instance) {
        mInstances.Append(instance);
    }

    void Client::TrackObject(Adapter* adapter) {
        mAdapters.Append(adapter);
    }

    void Client::TrackObject(Device* device) {
        mDevices.Append(device);
    }

    void Client::TrackObject(Surface* surface) {
        // Surfaces in the wire are not implemented.
        ASSERT(false);
    }

    bool Client::IsDisconnected() const {
        return mDisconnected;
    }

}}  // namespace dawn_wire::client
