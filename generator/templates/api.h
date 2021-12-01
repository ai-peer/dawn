//* Copyright 2020 The Dawn Authors
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
//*
//*
//* This template itself is part of the Dawn source and follows Dawn's license
//* but the generated file is used for "Web API native". The template comments
//* using //* at the top of the file are removed during generation such that
//* the resulting file starts with the BSD 3-Clause comment.
//*
//*
{% include '../LICENSE' %}
#ifndef {{metadata.api.upper()}}_H_
#define {{metadata.api.upper()}}_H_

{% set c_prefix = metadata.c_prefix %}
#if defined({{c_prefix}}_SHARED_LIBRARY)
#    if defined(_WIN32)
#        if defined({{c_prefix}}_IMPLEMENTATION)
#            define {{c_prefix}}_EXPORT __declspec(dllexport)
#        else
#            define {{c_prefix}}_EXPORT __declspec(dllimport)
#        endif
#    else  // defined(_WIN32)
#        if defined({{c_prefix}}_IMPLEMENTATION)
#            define {{c_prefix}}_EXPORT __attribute__((visibility("default")))
#        else
#            define {{c_prefix}}_EXPORT
#        endif
#    endif  // defined(_WIN32)
#else       // defined({{c_prefix}}_SHARED_LIBRARY)
#    define {{c_prefix}}_EXPORT
#endif  // defined({{c_prefix}}_SHARED_LIBRARY)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

{% for constant in by_category["constant"] %}
    #define {{c_prefix}}_{{constant.name.SNAKE_CASE()}} {{constant.value}}
{% endfor %}

typedef uint32_t {{c_prefix}}Flags;

{% for type in by_category["object"] %}
    typedef struct {{as_cType(type.name)}}Impl* {{as_cType(type.name)}};
{% endfor %}

{% for type in by_category["enum"] + by_category["bitmask"] %}
    typedef enum {{as_cType(type.name)}} {
        {% for value in type.values %}
            {{as_cEnum(type.name, value.name)}} = 0x{{format(value.value, "08X")}},
        {% endfor %}
        {{as_cEnum(type.name, Name("force32"))}} = 0x7FFFFFFF
    } {{as_cType(type.name)}};
    {% if type.category == "bitmask" %}
        typedef {{c_prefix}}Flags {{as_cType(type.name)}}Flags;
    {% endif %}

{% endfor -%}

typedef struct {{c_prefix}}ChainedStruct {
    struct {{c_prefix}}ChainedStruct const * next;
    {{c_prefix}}SType sType;
} {{c_prefix}}ChainedStruct;

typedef struct {{c_prefix}}ChainedStructOut {
    struct {{c_prefix}}ChainedStructOut * next;
    {{c_prefix}}SType sType;
} {{c_prefix}}ChainedStructOut;

{% for type in by_category["structure"] %}
    typedef struct {{as_cType(type.name)}} {
        {% set Out = "Out" if type.output else "" %}
        {% set const = "const " if not type.output else "" %}
        {% if type.extensible %}
            {{c_prefix}}ChainedStruct{{Out}} {{const}}* nextInChain;
        {% endif %}
        {% if type.chained %}
            {{c_prefix}}ChainedStruct{{Out}} chain;
        {% endif %}
        {% for member in type.members %}
            {{as_annotated_cType(member)}};
        {% endfor %}
    } {{as_cType(type.name)}};

{% endfor %}
{% for typeDef in by_category["typedef"] %}
    // {{as_cType(typeDef.name)}} is deprecated.
    // Use {{as_cType(typeDef.type.name)}} instead.
    typedef {{as_cType(typeDef.type.name)}} {{as_cType(typeDef.name)}};

{% endfor %}
#ifdef __cplusplus
extern "C" {
#endif

{% for type in by_category["callback"] %}
    typedef void (*{{as_cType(type.name)}})(
        {%- for arg in type.arguments -%}
            {% if not loop.first %}, {% endif %}{{as_annotated_cType(arg)}}
        {%- endfor -%}
    );
{% endfor %}

typedef void (*{{c_prefix}}Proc)(void);

#if !defined({{c_prefix}}_SKIP_PROCS)

typedef WGPUInstance (*WGPUProcCreateInstance)(WGPUInstanceDescriptor const * descriptor);
typedef WGPUProc (*WGPUProcGetProcAddress)(WGPUDevice device, char const * procName);

{% for type in by_category["object"] if len(c_methods(type)) > 0 %}
    // Procs of {{type.name.CamelCase()}}
    {% for method in c_methods(type) %}
        typedef {{as_cType(method.return_type.name)}} (*{{as_cProc(type.name, method.name)}})(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}

{% endfor %}
#endif  // !defined({{c_prefix}}_SKIP_PROCS)

#if !defined({{c_prefix}}_SKIP_DECLARATIONS)

WGPU_EXPORT WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor);
WGPU_EXPORT WGPUProc wgpuGetProcAddress(WGPUDevice device, char const * procName);

{% for type in by_category["object"] if len(c_methods(type)) > 0 %}
    // Methods of {{type.name.CamelCase()}}
    {% for method in c_methods(type) %}
        {{c_prefix}}_EXPORT {{as_cType(method.return_type.name)}} {{as_cMethod(type.name, method.name)}}(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}

{% endfor %}
#endif  // !defined({{c_prefix}}_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // {{metadata.api.upper()}}_H_
