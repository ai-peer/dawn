//* Copyright 2023 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.
{% set API = metadata.api.upper() %}
{% if 'dawn' in enabled_tags %}
    #ifdef __EMSCRIPTEN__
    #error "Do not include this header. Emscripten already provides headers needed for {{metadata.api}}."
    #endif
{% endif %}
#ifndef {{API}}_CPP_CHAINED_STRUCT_H_
#define {{API}}_CPP_CHAINED_STRUCT_H_

namespace {{metadata.namespace}} {

    namespace detail {
        constexpr size_t ConstexprMax(size_t a, size_t b) {
            return a > b ? a : b;
        }
    }  // namespace detail

    {% set s_types = types["s type"] %}
    enum class {{as_cppType(s_types.name)}} : uint32_t {
        {% for value in s_types.values %}
            {{as_cppEnum(value.name)}} = 0x{{format(value.value, "08X")}},
        {% endfor %}
    };

    struct ChainedStruct {
        ChainedStruct const * nextInChain = nullptr;
        SType sType = SType::Invalid;
    };

    struct ChainedStructOut {
        ChainedStructOut * nextInChain = nullptr;
        SType sType = SType::Invalid;
    };

}  // namespace {{metadata.namespace}}}

#endif // {{API}}_CPP_CHAINED_STRUCT_H_
