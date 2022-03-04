// Copyright 2022 The Dawn Authors
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

#ifndef DAWN_COMMON_SERIALIZE_H_
#define DAWN_COMMON_SERIALIZE_H_

#include "dawn/common/Blob.h"
#include "dawn/common/TypeID.h"
#include "dawn/common/traits/Serialize.h"

#include <memory>
#include <optional>

namespace dawn {

    // Serialized format is: | ArgCount, TypeIDs..., Args... |
    template <typename Sink, typename... Args>
    void Serialize(Sink* sink, const Args&... args) {
        size_t argCount = sizeof...(Args);
        size_t size = SerializeTraits<size_t>::Size(argCount) +
                      sizeof...(Args) * SerializeTraits<TypeID_t>::Size(0) +
                      (SerializeTraits<Args>::Size(args) + ...);

        char* ptr = sink->Alloc(size);
        SerializeTraits<size_t>::Write(&ptr, argCount);
        ((SerializeTraits<TypeID_t>::Write(&ptr, TypeID<Args>)), ...);
        ((SerializeTraits<Args>::Write(&ptr, args)), ...);
    }

}  // namespace dawn

#endif  // DAWN_COMMON_SERIALIZE_H_