//* Copyright 2017 The Dawn & Tint Authors
//*
//* Redistribution and use in source and binary forms, with or without
//* modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this
//*    list of conditions and the following disclaimer.
//*
//* 2. Redistributions in binary form must reproduce the above copyright notice,
//*    this list of conditions and the following disclaimer in the documentation
//*    and/or other materials provided with the distribution.
//*
//* 3. Neither the name of the copyright holder nor the names of its
//*    contributors may be used to endorse or promote products derived from
//*    this software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
{% set API = metadata.api.upper() %}
{% set api = API.lower() %}
{% if 'dawn' in enabled_tags %}
    #ifdef __EMSCRIPTEN__
    #error "Do not include this header. Emscripten already provides headers needed for {{metadata.api}}."
    #endif
{% endif %}

#ifndef {{API}}_CPP_ENUMS_H_
#define {{API}}_CPP_ENUMS_H_

#include "{{api}}/{{api}}.h"
#include "{{api}}/{{api}}_enum_class_bitmasks.h"

namespace std {
template <typename T>
struct hash;
}

namespace {{metadata.namespace}} {

    {% for type in by_category["enum"] %}
        {% set CppType = as_cppType(type.name) %}
        {% set CType = as_cType(type.name) %}
        enum class {{CppType}} : uint32_t {
            {% for value in type.values %}
                {{as_cppEnum(value.name)}} = {{as_cEnum(type.name, value.name)}},
            {% endfor %}
        };
        static_assert(sizeof({{CppType}}) == sizeof({{CType}}), "sizeof mismatch for {{CppType}}");
        static_assert(alignof({{CppType}}) == alignof({{CType}}), "alignof mismatch for {{CppType}}");

    {% endfor %}

    {% for type in by_category["bitmask"] %}
        {% set CppType = as_cppType(type.name) %}
        {% set CType = as_cType(type.name) + "Flags" %}
        enum class {{CppType}} : uint32_t {
            {% for value in type.values %}
                {{as_cppEnum(value.name)}} = {{as_cEnum(type.name, value.name)}},
            {% endfor %}
        };
        static_assert(sizeof({{CppType}}) == sizeof({{CType}}), "sizeof mismatch for {{CppType}}");
        static_assert(alignof({{CppType}}) == alignof({{CType}}), "alignof mismatch for {{CppType}}");

    {% endfor %}

    // Special class for booleans in order to allow implicit conversions.
    {% set BoolCppType = as_cppType(types["bool"].name) %}
    {% set BoolCType = as_cType(types["bool"].name) %}
    class {{BoolCppType}} {
      public:
        constexpr {{BoolCppType}}() = default;
        // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
        constexpr {{BoolCppType}}(bool value) : mValue(static_cast<{{BoolCType}}>(value)) {}
        // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
        {{BoolCppType}}({{BoolCType}} value): mValue(value) {}

        constexpr operator bool() const { return static_cast<bool>(mValue); }

      private:
        friend struct std::hash<{{BoolCppType}}>;
        // Default to false.
        {{BoolCType}} mValue = static_cast<{{BoolCType}}>(false);
    };

    {%- if metadata.namespace != 'wgpu' %}
        // The operators of webgpu_enum_class_bitmasks.h are in the wgpu:: namespace,
        // and need to be imported into this namespace for Argument Dependent Lookup.
        WGPU_IMPORT_BITMASK_OPERATORS
    {% endif %}

}  // {{metadata.namespace}}

namespace wgpu {
    {% for type in by_category["bitmask"] %}
        template<>
        struct IsWGPUBitmask<{{metadata.namespace}}::{{as_cppType(type.name)}}> {
            static constexpr bool enable = true;
        };

    {% endfor %}
} // namespace wgpu

namespace std {
// Custom boolean class needs corresponding hash function so that it appears as a transparent bool.
template <>
struct hash<{{metadata.namespace}}::{{as_cppType(types["bool"].name)}}> {
  public:
    size_t operator()(const {{metadata.namespace}}::{{as_cppType(types["bool"].name)}} &v) const {
        return hash<bool>()(v);
    }
};
}  // namespace std

#endif  // {{API}}_CPP_ENUMS_H_
