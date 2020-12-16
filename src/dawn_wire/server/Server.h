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

#ifndef DAWNWIRE_SERVER_SERVER_H_
#define DAWNWIRE_SERVER_SERVER_H_

#include "dawn_wire/ChunkedCommandSerializer.h"
#include "dawn_wire/server/ServerBase_autogen.h"

namespace dawn_wire { namespace server {

    class Server;
    class MemoryTransferService;

    struct CallbackUserdata {
        Server* const server;
        std::weak_ptr<bool> const serverIsAlive;

      private:
        friend class Server;
        CallbackUserdata() = delete;
        CallbackUserdata(Server* server, const std::shared_ptr<bool>& serverIsAlive)
            : server(server), serverIsAlive(serverIsAlive) {
        }
    };

    struct MapUserdata : CallbackUserdata {
        using CallbackUserdata::CallbackUserdata;

        ObjectHandle buffer;
        WGPUBuffer bufferObj;
        uint32_t requestSerial;
        uint64_t offset;
        uint64_t size;
        WGPUMapModeFlags mode;
        // TODO(enga): Use a tagged pointer to save space.
        std::unique_ptr<MemoryTransferService::ReadHandle> readHandle = nullptr;
        std::unique_ptr<MemoryTransferService::WriteHandle> writeHandle = nullptr;
    };

    struct ErrorScopeUserdata : CallbackUserdata {
        using CallbackUserdata::CallbackUserdata;

        // TODO(enga): ObjectHandle device;
        // when the wire supports multiple devices.
        uint64_t requestSerial;
    };

    struct FenceCompletionUserdata : CallbackUserdata {
        using CallbackUserdata::CallbackUserdata;

        ObjectHandle fence;
        uint64_t value;
    };

    struct FenceOnCompletionUserdata : CallbackUserdata {
        using CallbackUserdata::CallbackUserdata;

        ObjectHandle fence;
        uint64_t requestSerial;
    };

    struct CreateReadyPipelineUserData : CallbackUserdata {
        using CallbackUserdata::CallbackUserdata;

        uint64_t requestSerial;
        ObjectId pipelineObjectID;
    };

    class Server : public ServerBase {
      public:
        Server(WGPUDevice device,
               const DawnProcTable& procs,
               CommandSerializer* serializer,
               MemoryTransferService* memoryTransferService);
        ~Server() override;

        // ChunkedCommandHandler implementation
        const volatile char* HandleCommandsImpl(const volatile char* commands,
                                                size_t size) override;

        bool InjectTexture(WGPUTexture texture, uint32_t id, uint32_t generation);

        template <typename T,
                  typename Enable = std::enable_if<std::is_base_of<CallbackUserdata, T>::value>>
        std::unique_ptr<T> MakeUserdata() {
            return std::unique_ptr<T>(new T(this, mIsAlive));
        }

      private:
        template <typename Cmd>
        void SerializeCommand(const Cmd& cmd) {
            mSerializer.SerializeCommand(cmd);
        }

        template <typename Cmd, typename ExtraSizeSerializeFn>
        void SerializeCommand(const Cmd& cmd,
                              size_t extraSize,
                              ExtraSizeSerializeFn&& SerializeExtraSize) {
            mSerializer.SerializeCommand(cmd, extraSize, SerializeExtraSize);
        }


        // Error callbacks
        void OnUncapturedError(WGPUErrorType type, const char* message);
        void OnDeviceLost(const char* message, CallbackUserdata* userdata);
        void OnDevicePopErrorScope(WGPUErrorType type,
                                   const char* message,
                                   ErrorScopeUserdata* userdata);
        void OnBufferMapAsyncCallback(WGPUBufferMapAsyncStatus status, MapUserdata* userdata);
        void OnFenceCompletedValueUpdated(WGPUFenceCompletionStatus status,
                                          FenceCompletionUserdata* userdata);
        void OnFenceOnCompletion(WGPUFenceCompletionStatus status,
                                 FenceOnCompletionUserdata* userdata);
        void OnCreateReadyComputePipelineCallback(WGPUCreateReadyPipelineStatus status,
                                                  WGPUComputePipeline pipeline,
                                                  const char* message,
                                                  CreateReadyPipelineUserData* userdata);
        void OnCreateReadyRenderPipelineCallback(WGPUCreateReadyPipelineStatus status,
                                                 WGPURenderPipeline pipeline,
                                                 const char* message,
                                                 CreateReadyPipelineUserData* userdata);

#include "dawn_wire/server/ServerPrototypes_autogen.inc"

        WireDeserializeAllocator mAllocator;
        ChunkedCommandSerializer mSerializer;
        DawnProcTable mProcs;
        std::unique_ptr<MemoryTransferService> mOwnedMemoryTransferService = nullptr;
        MemoryTransferService* mMemoryTransferService = nullptr;

        std::shared_ptr<bool> mIsAlive;
    };

    std::unique_ptr<MemoryTransferService> CreateInlineMemoryTransferService();

    template <typename F>
    class ForwardToServer;

    template <typename R, typename... Args>
    class ForwardToServer<R (Server::*)(Args...)> {
      private:
        using UserdataT = typename std::remove_pointer<typename std::decay<decltype(
            std::get<sizeof...(Args) - 1>(std::declval<std::tuple<Args...>>()))>::type>::type;

        template <class T, class... Ts>
        struct UntypedCallbackImpl;
        template <std::size_t... I, class... Ts>
        struct UntypedCallbackImpl<std::index_sequence<I...>, Ts...> {
            template <R (Server::*Func)(Args...)>
            static auto ForwardToServer(
                typename std::tuple_element<I, std::tuple<Ts...>>::type... args,
                void* userdata) {
                std::unique_ptr<UserdataT> data(static_cast<UserdataT*>(userdata));
                if (data->serverIsAlive.expired()) {
                    return;
                }
                (data->server->*Func)(std::forward<decltype(args)>(args)..., data.get());
            }
        };

        using UntypedCallback =
            UntypedCallbackImpl<std::make_index_sequence<sizeof...(Args) - 1>, Args...>;

      public:
        template <R (Server::*Func)(Args...)>
        static auto Func() {
            return UntypedCallback::template ForwardToServer<Func>;
        }
    };

}}  // namespace dawn_wire::server

#endif  // DAWNWIRE_SERVER_SERVER_H_
