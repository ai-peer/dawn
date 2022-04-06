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

{% set impl_dir = metadata.impl_dir + "/" if metadata.impl_dir else "" %}
{% set namespace_name = Name(metadata.native_namespace) %}
{% set native_namespace = namespace_name.namespace_case() %}
{% set native_dir = impl_dir + namespace_name.Dirs() %}
{% set prefix = metadata.proc_table_prefix.lower() %}
#include "{{native_dir}}/CacheKey.h"
#include "{{native_dir}}/{{prefix}}_platform.h"

#include <cstring>

namespace {{native_namespace}} {

//
// Cache key serializers for wgpu structures used in caching.
//
{% macro render_serializer(member) %}
    {%- set name = member.name.camelCase() -%}
    {%- if member.length == None -%}
        .Record(t.{{name}})
    {%- elif member.length == "strlen" -%}
        .RecordIterable(t.{{name}}, strlen(t.{{name}}))
    {%- else -%}
        .RecordIterable(t.{{name}}, t.{{member.length.name.camelCase()}})
    {%- endif -%}
{% endmacro %}
{% macro render_serializers(members, omit) %}
    {% for member in members %}
        {%- if not member.name.get() in omit -%}
            {{render_serializer(member)}}
        {%- endif -%}
    {%- endfor %}
{% endmacro %}

template <>
void CacheKeySerializer<AdapterProperties>::Serialize(CacheKey* key, const AdapterProperties& t) {
    (*key){{render_serializers(types["adapter properties"].members, [])}};
}

template <>
void CacheKeySerializer<DeviceDescriptor>::Serialize(CacheKey* key, const DeviceDescriptor& t) {
    (*key){{
        render_serializers(
            types["device descriptor"].members,
            ["label", "required features count", "required limits", "default queue"]
        )
    }};
}

template <>
void CacheKeySerializer<DawnTogglesDeviceDescriptor>::Serialize(CacheKey* key, const DawnTogglesDeviceDescriptor& t) {
    // For the toggles we manually serialize because there are c-style strings involved.
    key->Record(static_cast<size_t>(t.forceEnabledTogglesCount));
    for (size_t i = 0; i < t.forceEnabledTogglesCount; i++) {
        key->RecordIterable(t.forceEnabledToggles[i], strlen(t.forceEnabledToggles[i]));
    }
    key->Record(static_cast<size_t>(t.forceDisabledTogglesCount));
    for (size_t i = 0; i < t.forceDisabledTogglesCount; i++) {
        key->RecordIterable(t.forceDisabledToggles[i], strlen(t.forceDisabledToggles[i]));
    }

    // Still call the default serializer with the omitted fields in case new fields are added.
    [[maybe_unused]]
    CacheKey& k = (*key){{
        render_serializers(
            types["dawn toggles device descriptor"].members,
            [
                "force enabled toggles count",
                "force enabled toggles",
                "force disabled toggles count",
                "force disabled toggles"
            ]
        )
    }};
}

template <>
void CacheKeySerializer<DawnCacheDeviceDescriptor>::Serialize(CacheKey* key, const DawnCacheDeviceDescriptor& t) {
    (*key){{render_serializers(types["dawn cache device descriptor"].members, [])}};
}

} // namespace {{native_namespace}}
