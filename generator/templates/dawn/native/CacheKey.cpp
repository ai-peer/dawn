//* Copyright 2022 The Dawn Authors
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

{% set impl_dir = metadata.impl_dir + "/" if metadata.impl_dir else "" %}
{% set namespace_name = Name(metadata.native_namespace) %}
{% set native_namespace = namespace_name.namespace_case() %}
{% set native_dir = impl_dir + namespace_name.Dirs() %}
{% set prefix = metadata.proc_table_prefix.lower() %}
#include "{{native_dir}}/{{prefix}}_platform.h"

#include "dawn/native/CacheKey.h"

namespace {{native_namespace}} {

    //
    // Enums (Declarations are not necessarily in dawn::native)
    //    Note: Currently serialized as strings of the numeric value. May change to binary format in
    //          the future once we understand how 0 would work w.r.t platform cache keys.
    //
    {% set namespace = metadata.namespace %}

    {% for type in by_category["enum"] %}
        {% set T = "::" + namespace + "::" + as_cppType(type.name) %}
        template <>
        void CacheKeySerializer<{{T}}>::Serialize(CacheKey* key, const {{T}}& value) {
            SerializeInto(key, static_cast<typename std::underlying_type<{{T}}>::type>(value));
        }
    {% endfor %}

    //
    // Bitmasks (Declarations are not necessarily in dawn::native)
    //    Note: Currently serialized as strings of the numeric value. May change to binary format in
    //          the future once we understand how 0 would work w.r.t platform cache keys.
    //

    {% for type in by_category["bitmask"] %}
        {% set T = "::" + namespace + "::" + as_cppType(type.name) %}
        template <>
        void CacheKeySerializer<{{T}}>::Serialize(CacheKey* key, const {{T}}& value) {
            SerializeInto(key, static_cast<typename std::underlying_type<{{T}}>::type>(value));
        }
    {% endfor %}

    //
    // Structures
    //

    {% for type in by_category["structure"] %}
        {% if type.name.get() in [
             "buffer binding layout",
             "sampler binding layout",
             "texture binding layout",
             "storage texture binding layout"
           ]
        %}
        {% set T = as_cppType(type.name) %}
        template <>
        void CacheKeySerializer<{{T}}>::Serialize(CacheKey* key, const {{T}}& value) {
            {% set members = [] %}
            {% for member in type.members %}
                {% set memberName = member.name.camelCase() %}
                {% do members.append("value." + memberName) %}
            {% endfor %}
            SerializeInto(key, GetCacheKey({{members|join(", ")}}));
        }
        {% endif %}
    {% endfor %}

} // namespace {{native_namespace}}
