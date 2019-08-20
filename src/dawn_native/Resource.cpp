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

#include "dawn_native/Resource.h"

namespace dawn_native {
    ResourceBase::ResourceBase(uint64_t offset, AllocationMethod method)
        : mMethod(method), mOffset(offset) {
    }

    uint64_t ResourceBase::GetOffset() const {
        return mOffset;
    }

    AllocationMethod ResourceBase::GetAllocationMethod() const {
        return mMethod;
    }
}  // namespace dawn_native