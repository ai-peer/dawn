// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dawn/wire/ChunkedCommandHandler.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/ApiObjects.h"

namespace dawn::wire {

class FuzzObjectIdProvider : public ObjectIdProvider {
  private:
    // Implementation of the ObjectIdProvider interface
    WireResult GetId(WGPUAdapter object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUAdapter object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUBindGroup object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUBindGroup object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUBindGroupLayout object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUBindGroupLayout object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUBuffer object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUBuffer object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUCommandBuffer object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUCommandBuffer object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUCommandEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUCommandEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUComputePassEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUComputePassEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUComputePipeline object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUComputePipeline object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUDevice object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUDevice object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUExternalTexture object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUExternalTexture object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUInstance object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUInstance object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUPipelineLayout object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUPipelineLayout object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUQuerySet object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUQuerySet object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUQueue object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUQueue object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPURenderBundle object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPURenderBundle object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPURenderBundleEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPURenderBundleEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPURenderPassEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPURenderPassEncoder object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPURenderPipeline object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPURenderPipeline object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUSampler object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUSampler object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUShaderModule object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUShaderModule object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUSurface object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUSurface object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUSwapChain object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUSwapChain object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUTexture object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUTexture object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetId(WGPUTextureView object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
    WireResult GetOptionalId(WGPUTextureView object, ObjectId* out) const final {
        *out = reinterpret_cast<uintptr_t>(object);
        return WireResult::Success;
    }
};

}  // namespace dawn::wire
