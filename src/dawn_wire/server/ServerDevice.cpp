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

#include "dawn_wire/server/Server.h"

namespace dawn_wire { namespace server {

    void Server::ForwardUncapturedError(WGPUErrorType type, const char* message, void* userdata) {
        auto* data = static_cast<ObjectData<WGPUDevice>*>(userdata);
        ObjectHandle device(data->id, data->generation);
        data->server->OnUncapturedError(device, type, message);
    }

    void Server::ForwardDeviceLost(const char* message, void* userdata) {
        auto* data = static_cast<ObjectData<WGPUDevice>*>(userdata);
        ObjectHandle device(data->id, data->generation);
        data->server->OnDeviceLost(device, message);
    }

    void Server::ForwardCreateReadyComputePipeline(WGPUCreateReadyPipelineStatus status,
                                                   WGPUComputePipeline pipeline,
                                                   const char* message,
                                                   void* userdata) {
        std::unique_ptr<CreateReadyPipelineUserData> createReadyPipelineUserData(
            static_cast<CreateReadyPipelineUserData*>(userdata));

        // We need to ensure createReadyPipelineUserData->server is still pointing to a valid
        // object before doing any operations on it.
        if (createReadyPipelineUserData->isServerAlive.expired()) {
            return;
        }

        createReadyPipelineUserData->server->OnCreateReadyComputePipelineCallback(
            status, pipeline, message, createReadyPipelineUserData.release());
    }

    void Server::ForwardCreateReadyRenderPipeline(WGPUCreateReadyPipelineStatus status,
                                                  WGPURenderPipeline pipeline,
                                                  const char* message,
                                                  void* userdata) {
        std::unique_ptr<CreateReadyPipelineUserData> createReadyPipelineUserData(
            static_cast<CreateReadyPipelineUserData*>(userdata));

        // We need to ensure createReadyPipelineUserData->server is still pointing to a valid
        // object before doing any operations on it.
        if (createReadyPipelineUserData->isServerAlive.expired()) {
            return;
        }

        createReadyPipelineUserData->server->OnCreateReadyRenderPipelineCallback(
            status, pipeline, message, createReadyPipelineUserData.release());
    }

    void Server::OnUncapturedError(ObjectHandle device, WGPUErrorType type, const char* message) {
        ReturnDeviceUncapturedErrorCallbackCmd cmd;
        cmd.device = device;
        cmd.type = type;
        cmd.message = message;

        SerializeCommand(cmd);
    }

    void Server::OnDeviceLost(ObjectHandle device, const char* message) {
        ReturnDeviceLostCallbackCmd cmd;
        cmd.device = device;
        cmd.message = message;

        SerializeCommand(cmd);
    }

    bool Server::DoDevicePopErrorScope(ObjectId deviceId, uint64_t requestSerial) {
        // The null object isn't valid as `self`
        if (deviceId == 0) {
            return false;
        }

        auto* deviceData = DeviceObjects().Get(deviceId);
        if (deviceData == nullptr) {
            return false;
        }

        ErrorScopeUserdata* userdata = new ErrorScopeUserdata;
        userdata->server = this;
        userdata->device = ObjectHandle{deviceId, deviceData->generation};
        userdata->requestSerial = requestSerial;

        bool success =
            mProcs.devicePopErrorScope(deviceData->handle, ForwardPopErrorScope, userdata);
        if (!success) {
            delete userdata;
        }
        return success;
    }

    bool Server::DoDeviceCreateReadyComputePipeline(
        ObjectId deviceId,
        uint64_t requestSerial,
        ObjectHandle pipelineObjectHandle,
        const WGPUComputePipelineDescriptor* descriptor) {
        // The null object isn't valid as `self`
        if (deviceId == 0) {
            return false;
        }

        auto* deviceData = DeviceObjects().Get(deviceId);
        if (deviceData == nullptr) {
            return false;
        }

        auto* resultData = ComputePipelineObjects().Allocate(pipelineObjectHandle.id);
        if (resultData == nullptr) {
            return false;
        }

        resultData->generation = pipelineObjectHandle.generation;

        std::unique_ptr<CreateReadyPipelineUserData> userdata =
            std::make_unique<CreateReadyPipelineUserData>();
        userdata->isServerAlive = mIsAlive;
        userdata->server = this;
        userdata->device = ObjectHandle{deviceId, deviceData->generation};
        userdata->requestSerial = requestSerial;
        userdata->pipelineObjectID = pipelineObjectHandle.id;

        mProcs.deviceCreateReadyComputePipeline(
            deviceData->handle, descriptor, ForwardCreateReadyComputePipeline, userdata.release());
        return true;
    }

    void Server::OnCreateReadyComputePipelineCallback(WGPUCreateReadyPipelineStatus status,
                                                      WGPUComputePipeline pipeline,
                                                      const char* message,
                                                      CreateReadyPipelineUserData* userdata) {
        std::unique_ptr<CreateReadyPipelineUserData> data(userdata);

        auto* computePipelineObject = ComputePipelineObjects().Get(data->pipelineObjectID);
        ASSERT(computePipelineObject != nullptr);

        switch (status) {
            case WGPUCreateReadyPipelineStatus_Success:
                computePipelineObject->handle = pipeline;
                break;

            case WGPUCreateReadyPipelineStatus_Error:
                ComputePipelineObjects().Free(data->pipelineObjectID);
                break;

            // Currently this code is unreachable because WireServer is always deleted before the
            // removal of the device. In the future this logic may be changed when we decide to
            // support sharing one pair of WireServer/WireClient to multiple devices.
            case WGPUCreateReadyPipelineStatus_DeviceLost:
            case WGPUCreateReadyPipelineStatus_DeviceDestroyed:
            case WGPUCreateReadyPipelineStatus_Unknown:
            default:
                UNREACHABLE();
        }

        ReturnDeviceCreateReadyComputePipelineCallbackCmd cmd;
        cmd.status = status;
        cmd.device = data->device;
        cmd.requestSerial = data->requestSerial;
        cmd.message = message;

        SerializeCommand(cmd);
    }

    bool Server::DoDeviceCreateReadyRenderPipeline(ObjectId deviceId,
                                                   uint64_t requestSerial,
                                                   ObjectHandle pipelineObjectHandle,
                                                   const WGPURenderPipelineDescriptor* descriptor) {
        // The null object isn't valid as `self`
        if (deviceId == 0) {
            return false;
        }

        auto* deviceData = DeviceObjects().Get(deviceId);
        if (deviceData == nullptr) {
            return false;
        }

        auto* resultData = RenderPipelineObjects().Allocate(pipelineObjectHandle.id);
        if (resultData == nullptr) {
            return false;
        }

        resultData->generation = pipelineObjectHandle.generation;

        std::unique_ptr<CreateReadyPipelineUserData> userdata =
            std::make_unique<CreateReadyPipelineUserData>();
        userdata->isServerAlive = mIsAlive;
        userdata->server = this;
        userdata->device = ObjectHandle{deviceId, deviceData->generation};
        userdata->requestSerial = requestSerial;
        userdata->pipelineObjectID = pipelineObjectHandle.id;

        mProcs.deviceCreateReadyRenderPipeline(
            deviceData->handle, descriptor, ForwardCreateReadyRenderPipeline, userdata.release());
        return true;
    }

    void Server::OnCreateReadyRenderPipelineCallback(WGPUCreateReadyPipelineStatus status,
                                                     WGPURenderPipeline pipeline,
                                                     const char* message,
                                                     CreateReadyPipelineUserData* userdata) {
        std::unique_ptr<CreateReadyPipelineUserData> data(userdata);

        auto* renderPipelineObject = RenderPipelineObjects().Get(data->pipelineObjectID);
        ASSERT(renderPipelineObject != nullptr);

        switch (status) {
            case WGPUCreateReadyPipelineStatus_Success:
                renderPipelineObject->handle = pipeline;
                break;

            case WGPUCreateReadyPipelineStatus_Error:
                RenderPipelineObjects().Free(data->pipelineObjectID);
                break;

            // Currently this code is unreachable because WireServer is always deleted before the
            // removal of the device. In the future this logic may be changed when we decide to
            // support sharing one pair of WireServer/WireClient to multiple devices.
            case WGPUCreateReadyPipelineStatus_DeviceLost:
            case WGPUCreateReadyPipelineStatus_DeviceDestroyed:
            case WGPUCreateReadyPipelineStatus_Unknown:
            default:
                UNREACHABLE();
        }

        ReturnDeviceCreateReadyRenderPipelineCallbackCmd cmd;
        cmd.status = status;
        cmd.device = data->device;
        cmd.requestSerial = data->requestSerial;
        cmd.message = message;

        SerializeCommand(cmd);
    }

    // static
    void Server::ForwardPopErrorScope(WGPUErrorType type, const char* message, void* userdata) {
        auto* data = reinterpret_cast<ErrorScopeUserdata*>(userdata);
        data->server->OnDevicePopErrorScope(type, message, data);
    }

    void Server::OnDevicePopErrorScope(WGPUErrorType type,
                                       const char* message,
                                       ErrorScopeUserdata* userdata) {
        std::unique_ptr<ErrorScopeUserdata> data{userdata};

        ReturnDevicePopErrorScopeCallbackCmd cmd;
        cmd.requestSerial = data->requestSerial;
        cmd.device = data->device;
        cmd.type = type;
        cmd.message = message;

        SerializeCommand(cmd);
    }

}}  // namespace dawn_wire::server
