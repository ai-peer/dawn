

#include <algorithm>

#include "dawn/fuzzers/lpmfuzz/DawnLPMConstants.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMObjectStore.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializerCustom.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializer_autogen.h"
#include "dawn/webgpu.h"
#include "dawn/wire/ChunkedCommandSerializer.h"
#include "dawn/wire/ObjectType_autogen.h"

namespace dawn::wire {

void GetCustomSerializedData(const fuzzing::Command& command,
                             dawn::wire::ChunkedCommandSerializer serializer,
                             ityp::array<ObjectType, client::DawnLPMObjectStore, 24>& gObjectStores,
                             DawnLPMObjectIdProvider& provider) {
    switch (command.command_case()) {
        case fuzzing::Command::kDeviceCreateShaderModule: {
            DeviceCreateShaderModuleCmd cmd;
            memset(&cmd, 0, sizeof(DeviceCreateShaderModuleCmd));

            ObjectId cmd_self_id =
                gObjectStores[ObjectType::Device].Get(command.devicecreateshadermodule().self());

            if (cmd_self_id == static_cast<ObjectId>(INVALID_OBJECTID)) {
                break;
            }

            cmd.self = reinterpret_cast<WGPUDevice>(cmd_self_id);

            WGPUShaderModuleDescriptor cmd_descriptor;
            memset(&cmd_descriptor, 0, sizeof(struct WGPUShaderModuleDescriptor));

            WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
            wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;

            // Hardcoded shader for now, eventually we should write an LPM grammar for WGSL
            wgsl_desc.source =
                "@group(0) @binding(0) \
                var<storage, read_write> output: array<f32>; \
                @compute @workgroup_size(64) \
                fn main() { \
                    output[0] = 0;\
                }";
            cmd_descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);

            cmd.descriptor = &cmd_descriptor;
            if (gObjectStores[ObjectType::ShaderModule].size() >= SHADER_MODULE_LIMIT) {
                break;
            }

            cmd.result = gObjectStores[ObjectType::ShaderModule].ReserveHandle();
            serializer.SerializeCommand(cmd, provider);
            break;
        }
        case fuzzing::Command::kDeviceCreateRenderPipeline: {
            break;
        }
        case fuzzing::Command::kDeviceCreateRenderPipelineAsync: {
            break;
        }
        case fuzzing::Command::kDestroyObject: {
            DestroyObjectCmd cmd;
            memset(&cmd, 0, sizeof(DestroyObjectCmd));

            cmd.objectType = static_cast<ObjectType>(command.destroyobject().objecttype() % 24);

            cmd.objectId = gObjectStores[static_cast<ObjectType>(cmd.objectType)].Get(
                command.destroyobject().objectid());

            if (cmd.objectId == static_cast<ObjectId>(INVALID_OBJECTID)) {
                break;
            }

            gObjectStores[cmd.objectType].Free(cmd.objectId);
            serializer.SerializeCommand(cmd, provider);
            break;
        }
        default: {
            // All Wire Commands in the generated protobuf file should be handled by
            // the switch statement.
            // ASSERT(false);
            break;
        }
    }
}

}  // namespace dawn::wire