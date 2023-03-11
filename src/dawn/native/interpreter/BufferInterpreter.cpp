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

#include "dawn/native/interpreter/BufferInterpreter.h"

#include <utility>

#include "dawn/native/ChainUtils.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/interpreter/DeviceInterpreter.h"
#include "tint/interp/memory.h"

namespace dawn::native::interpreter {

// static
ResultOrError<Ref<Buffer>> Buffer::Create(Device* device,
                                          const UnpackedPtr<BufferDescriptor>& descriptor) {
    Ref<Buffer> buffer = AcquireRef(new Buffer(device, descriptor));
    DAWN_TRY(buffer->Initialize(descriptor->mappedAtCreation));
    return std::move(buffer);
}

Buffer::~Buffer() {}

tint::interp::Memory& Buffer::GetMemory() {
    return *mMemory;
}

MaybeError Buffer::Initialize(bool mappedAtCreation) {
    mAllocatedSize = GetSize();

    // Limit buffer sizes to 16GB.
    if (mAllocatedSize >= (uint64_t(1) << 34)) {
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer size exceeds 16GB");
    }

    mMemory = std::make_unique<tint::interp::Memory>(mAllocatedSize);
    return {};
}

MaybeError Buffer::MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) {
    return {};
}

void Buffer::UnmapImpl() {}

bool Buffer::IsCPUWritableAtCreation() const {
    return true;
}

MaybeError Buffer::MapAtCreationImpl() {
    return {};
}

void* Buffer::GetMappedPointer() {
    return mMemory->Data();
}

}  // namespace dawn::native::interpreter
