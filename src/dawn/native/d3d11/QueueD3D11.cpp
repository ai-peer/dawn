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

#include "dawn/native/d3d11/QueueD3D11.h"

#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/CommandBufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

Ref<Queue> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    return queue;
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    [[maybe_unused]] Device* device = ToBackend(GetDevice());

    TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferGL::Execute");
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->Execute());
    }
    TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferGL::Execute");

    // device->SubmitFenceSync();
    return {};
}

MaybeError Queue::WriteBufferImpl(BufferBase* buffer,
                                  uint64_t bufferOffset,
                                  const void* data,
                                  size_t size) {
    ID3D11Buffer* d3d11Buffer = ToBackend(buffer)->GetD3D11Buffer();
    if (d3d11Buffer) {
        CommandRecordingContext* commandRecordingContext;
        DAWN_TRY_ASSIGN(commandRecordingContext,
                        ToBackend(GetDevice())->GetPendingCommandContext());

        ID3D11DeviceContext1* d3d11DeviceContext1 =
            commandRecordingContext->GetD3D11DeviceContext1();
        D3D11_BOX dstBox = {};
        dstBox.left = static_cast<UINT>(bufferOffset);
        dstBox.right = static_cast<UINT>(bufferOffset + size);
        dstBox.bottom = 1;
        dstBox.back = 1;
        d3d11DeviceContext1->UpdateSubresource(ToBackend(buffer)->GetD3D11Buffer(), 0, &dstBox,
                                               data, 0, 0);
    } else {
        // For staging buffers, we can just memcpy the data into the buffer.
        memcpy(ToBackend(buffer)->GetStagingBufferPointer() + bufferOffset, data, size);
    }
    // TODO: mark buffer is initialized.
    return {};
}

MaybeError Queue::WriteTextureImpl(const ImageCopyTexture& destination,
                                   const void* data,
                                   const TextureDataLayout& dataLayout,
                                   const Extent3D& writeSizePixel) {
    // DAWN_INVALID_IF(destination.aspect == wgpu::TextureAspect::StencilOnly,
    //                 "Writes to stencil textures unsupported on the OpenGL backend.");

    // TextureCopy textureCopy;
    // textureCopy.texture = destination.texture;
    // textureCopy.mipLevel = destination.mipLevel;
    // textureCopy.origin = destination.origin;
    // textureCopy.aspect = SelectFormatAspects(destination.texture->GetFormat(),
    // destination.aspect);

    // SubresourceRange range = GetSubresourcesAffectedByCopy(textureCopy, writeSizePixel);
    // if (IsCompleteSubresourceCopiedTo(destination.texture, writeSizePixel, destination.mipLevel))
    // {
    //     destination.texture->SetIsSubresourceContentInitialized(true, range);
    // } else {
    //     ToBackend(destination.texture)->EnsureSubresourceContentInitialized(range);
    // }
    // DoTexSubImage(ToBackend(GetDevice())->GetGL(), textureCopy, data, dataLayout,
    // writeSizePixel); ToBackend(destination.texture)->Touch();
    return {};
}

}  // namespace dawn::native::d3d11
