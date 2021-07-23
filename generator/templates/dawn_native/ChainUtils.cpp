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

#include "dawn_native/ChainUtils_autogen.h"

#include <unordered_set>

namespace dawn_native {

{% for value in types["s type"].values %}
    {% if value.valid %}
        void FindInChain(const ChainedStruct* chain, const {{as_cppEnum(value.name)}}** out) {
            for (; chain; chain = chain->nextInChain) {
                if (chain->sType == wgpu::SType::{{as_cppEnum(value.name)}}) {
                    *out = static_cast<const {{as_cppEnum(value.name)}}*>(chain);
                    break;
                }
            }
        }
    {% endif %}
{% endfor %}

}  // namespace dawn_native
