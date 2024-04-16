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

#ifndef {{API}}_CPP_STRUCTS_H_
#define {{API}}_CPP_STRUCTS_H_

#include <cmath>

#include "{{api}}/{{api}}.h"
#include "{{api}}/{{api}}_cpp_chained_struct.h"
#include "dawn/{{api}}_cpp_enums.h"
#include "dawn/{{api}}_cpp_objects.h"

namespace {{metadata.namespace}} {
    {% macro render_cpp_default_value(member, is_struct, force_default=False) -%}
        {%- if member.json_data.get("no_default", false) -%}
        {%- elif member.annotation in ["*", "const*"] and member.optional or member.default_value == "nullptr" -%}
            {{" "}}= nullptr
        {%- elif member.type.category == "object" and member.optional and is_struct -%}
            {{" "}}= nullptr
        {%- elif member.type.category in ["enum", "bitmask"] and member.default_value != None -%}
            {{" "}}= {{as_cppType(member.type.name)}}::{{as_cppEnum(Name(member.default_value))}}
        {%- elif member.type.category == "native" and member.default_value != None -%}
            {{" "}}= {{member.default_value}}
        {%- elif member.default_value != None -%}
            {{" "}}= {{member.default_value}}
        {%- else -%}
            {{assert(member.default_value == None)}}
            {%- if force_default -%}
                {{" "}}= {}
            {%- endif -%}
        {%- endif -%}
    {%- endmacro %}

    namespace detail {
        constexpr size_t ConstexprMax(size_t a, size_t b) {
            return a > b ? a : b;
        }
    }  // namespace detail

     // ChainedStruct
    {% set c_prefix = metadata.c_prefix %}
    static_assert(sizeof(ChainedStruct) == sizeof({{c_prefix}}ChainedStruct),
            "sizeof mismatch for ChainedStruct");
    static_assert(alignof(ChainedStruct) == alignof({{c_prefix}}ChainedStruct),
            "alignof mismatch for ChainedStruct");
    static_assert(offsetof(ChainedStruct, nextInChain) == offsetof({{c_prefix}}ChainedStruct, next),
            "offsetof mismatch for ChainedStruct::nextInChain");
    static_assert(offsetof(ChainedStruct, sType) == offsetof({{c_prefix}}ChainedStruct, sType),
            "offsetof mismatch for ChainedStruct::sType");

    {% for type in by_category["structure"] %}
        {% set Out = "Out" if type.output else "" %}
        {% set const = "const" if not type.output else "" %}
        {% if type.chained %}
            {% for root in type.chain_roots %}
                // Can be chained in {{as_cppType(root.name)}}
            {% endfor %}
            struct {{as_cppType(type.name)}} : ChainedStruct{{Out}} {
                {{as_cppType(type.name)}}() {
                    sType = SType::{{type.name.CamelCase()}};
                }
        {% else %}
            struct {{as_cppType(type.name)}} {
                {% if type.has_free_members_function %}
                    {{as_cppType(type.name)}}() = default;
                {% endif %}
        {% endif %}
            {% if type.has_free_members_function %}
                inline ~{{as_cppType(type.name)}}();
                {{as_cppType(type.name)}}(const {{as_cppType(type.name)}}&) = delete;
                {{as_cppType(type.name)}}& operator=(const {{as_cppType(type.name)}}&) = delete;
                inline {{as_cppType(type.name)}}({{as_cppType(type.name)}}&&);
                inline {{as_cppType(type.name)}}& operator=({{as_cppType(type.name)}}&&);
            {% endif %}
            {% if type.extensible %}
                ChainedStruct{{Out}} {{const}} * nextInChain = nullptr;
            {% endif %}
            {% for member in type.members %}
                {% set member_declaration = as_annotated_cppType(member, type.has_free_members_function) + render_cpp_default_value(member, True, type.has_free_members_function) %}
                {% if type.chained and loop.first %}
                    //* Align the first member after ChainedStruct to match the C struct layout.
                    //* It has to be aligned both to its natural and ChainedStruct's alignment.
                    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct{{out}}), alignof({{decorate("", as_cppType(member.type.name), member)}}));
                    alignas(kFirstMemberAlignment) {{member_declaration}};
                {% else %}
                    {{member_declaration}};
                {% endif %}
            {% endfor %}
            {% if type.has_free_members_function %}

              private:
                static inline void Reset({{as_cppType(type.name)}}& value);
            {% endif %}
        };

    {% endfor %}

}  // {{metadata.namespace}}

#endif  // {{API}}_CPP_STRUCTS_H_
