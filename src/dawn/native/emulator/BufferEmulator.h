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

#ifndef SRC_DAWN_NATIVE_EMULATOR_BUFFEREMULATOR_H_
#define SRC_DAWN_NATIVE_EMULATOR_BUFFEREMULATOR_H_

#include "dawn/native/Buffer.h"

#include <memory>

#include "tint/interp/memory.h"

namespace dawn::native::emulator {

class Device;

class Buffer final : public BufferBase {
  public:
    static ResultOrError<Ref<Buffer>> Create(Device* device, const BufferDescriptor* descriptor);

    tint::interp::Memory& Get() { return *mMemory; }

  private:
    ~Buffer() override;
    using BufferBase::BufferBase;
    MaybeError Initialize(bool mappedAtCreation);

    MaybeError MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) override;
    void UnmapImpl() override;
    bool IsCPUWritableAtCreation() const override;
    MaybeError MapAtCreationImpl() override;
    void* GetMappedPointer() override;

    std::unique_ptr<tint::interp::Memory> mMemory;
};

}  // namespace dawn::native::emulator

#endif  // SRC_DAWN_NATIVE_EMULATOR_BUFFEREMULATOR_H_
