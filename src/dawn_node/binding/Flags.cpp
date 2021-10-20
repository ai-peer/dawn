// Copyright 2021 The Dawn Authors
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

#include "src/dawn_node/binding/Flags.h"

#include <unordered_map>

namespace wgpu { namespace binding {
    namespace {
        std::unordered_map<std::string, std::string> flags;
    }

    void Flags::Set(std::string key, std::string value) {
        flags[key] = value;
    }

    std::optional<std::string> Flags::Get(const std::string& key) const {
        auto iter = flags.find(key);
        if (iter != flags.end()) {
            return iter->second;
        }
        return {};
    }
}}  // namespace wgpu::binding
