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

#include <cstdint>
#include <string>
#include <vector>

#include <spirv-tools/libspirv.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < sizeof(spv_target_env) + 1)
        return 0;

    const spv_context context = spvContextCreate(*reinterpret_cast<const spv_target_env*>(data));
    if (context == nullptr)
        return 0;

    data += sizeof(spv_target_env);
    size -= sizeof(spv_target_env);
    std::vector<uint32_t> input(data, data + (4 * (size / 4)));

    spvBinaryParse(context, nullptr, input.data(), input.size(), nullptr, nullptr, nullptr);

    spvContextDestroy(context);
    return 0;
}
