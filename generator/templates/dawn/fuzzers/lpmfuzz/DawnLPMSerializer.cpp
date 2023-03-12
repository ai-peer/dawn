// Copyright 2023 The Dawn Authors
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

#include "dawn/fuzzers/lpmfuzz/DawnLPMConstants.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializerCustom.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializer_autogen.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMFuzzer.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMObjectStore.h"
#include "dawn/wire/Wire.h"
#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/ApiObjects_autogen.h"
#include "dawn/webgpu.h"
#include "dawn/wire/client/Client.h"

#define SAFE_CLAMP(a) std::min(static_cast<uint64_t>(a), static_cast<uint64_t>(CLAMP_SIZE))

{% macro expand(member_access_pairs) -%}
    {%- for member_access_pair in member_access_pairs -%}
        {%- set member = member_access_pair[0] -%}
        {%- set access = member_access_pair[1] -%}

        .{{- as_protobufMemberNameLPM(member.name) }}({{ access if access }})
    {%- endfor -%}
{%- endmacro %}

{% macro expand_command(command, member_access_pairs) -%}
    command.{{ command.name.concatcase() }}(){{ expand(member_access_pairs) }}
{%- endmacro %}

{% macro expand_repeated_command_size(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    command.{{ command.name.concatcase() }}(){{ expand(member_access_pairs) }}.size()
{%- endmacro %}

{% macro expand_repeated_command_length(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    command.{{ command.name.concatcase() }}(){{ expand(member_access_pairs[:-1]) }}.{{ member.length.name.concatcase() }}()
{%- endmacro %}

{% macro expand_enum(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    static_cast<{{ as_cType(member.type.name) }}>(
        {{ expand_command(command, member_access_pairs) }}
    )
{%- endmacro %}

{% macro expand_bitmask(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    static_cast<{{ as_cType(member.type.name) }}>(
        {{ expand_command(command, member_access_pairs) }}
    )
{%- endmacro %}

{% macro expand_primitive(command, member_access_pairs) -%}
    {{ expand(command, member_access_pairs) }}
{%- endmacro %}

{% macro expand_string(command, member_access_pairs) -%}
    "main"
{%- endmacro %}

{% macro expand_return_object_type(command, member_access_pairs) -%}
    {% set type_name = command.lpm_info["returns"].CamelCase() %}
    gObjectStores[ObjectType::{{ type_name }}]
{%- endmacro %}

{% macro expand_return_object_limit(command, member_access_pairs) -%}
    {{ command.lpm_info["returns"].SNAKE_CASE() }}_LIMIT
{%- endmacro %}

{% macro expand_object_type(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set type_name =  member.type.name.CamelCase() %}

    gObjectStores[ObjectType::{{ type_name }}]
{%- endmacro %}

{% macro expand_object_id_type(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set type_name = member.lpm_info["type"].CamelCase() %}

    gObjectStores[ObjectType::{{ type_name }}]
{%- endmacro %}


{% macro assign_override(var, access, override) %}
    {{ var }}{{ access }} = {{ override }};
{% endmacro %}

{% macro assign_enum(var, access, command, member_access_pairs) -%}
    {{ var }}{{ access }} = {{ expand_enum(command, member_access_pairs) }};
{%- endmacro %}

{% macro assign_generic_value(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if "clamp" in member.lpm_info %}
         {{ var }}{{ access }} = SAFE_CLAMP({{ expand_command(command, member_access_pairs) }});
    {% else %}
        {{ var }}{{ access }} = {{ expand_command(command, member_access_pairs) }};
    {% endif %}
{%- endmacro %}

{% macro assign_string(var, access, command, member_access_pairs) -%}
    {{ var }}{{ access }} = {{ expand_string(command, member_access_pairs) }};
{%- endmacro %}

{% macro assign_float_array(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set array_var = as_varLocalArrayLPM(var, member) %}
    {% set varlen = as_varSizeLPM(var, member) %}

    size_t {{ varlen }} = 1024;
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }} ]
    );
    memset({{ array_var }}.get(), 0x41, {{ varlen }} * sizeof({{ as_cType(member.type.name) }}));

    {{ var }}{{ access }} = {{ array_var }}.get();
{%- endmacro %}

{% macro assign_byte_array(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set array_var = as_varLocalArrayLPM(var, member) %}
    {% set varlen = as_varSizeLPM(var, member) %}

    size_t {{ varlen }} = 1024;
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }} ]
    );
    memset({{ array_var }}.get(), 0x41, {{ varlen }});

    {{ var }}{{ access }} = {{ array_var }}.get();
{%- endmacro %}

{% macro assign_bitmask(var, access, command, member_access_pairs) -%}
    {% set bmlen = command.name.concatcase() + '_bmlen' %}

    size_t {{ bmlen }} = {{ expand_repeated_command_size(command, member_access_pairs) }};
    for (size_t bm = 0; bm < {{ bmlen }}; bm++) {
        {% set tmp = member_access_pairs.pop() %}
        {% do member_access_pairs.append((tmp[0], 'bm')) %}

        {{ var }}{{ access }} |= {{ expand_bitmask(command, member_access_pairs) }};
    }
{%- endmacro %}

{% macro assign_object(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set object_id = as_varIdLPM(var, member) %}
    ObjectId {{ object_id }} = {{ expand_object_type(command, member_access_pairs) }}.Get(
        {{ expand_command(command, member_access_pairs) }}
    );

    if ( {{ object_id }} != static_cast<ObjectId>(INVALID_OBJECTID) ) {
        {{ var }}{{ access }} = reinterpret_cast<{{ as_cType(member.type.name) }}>(
            {{ object_id }}
        );
    }
{%- endmacro %}

{% macro assign_object_id(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if "type" in member.lpm_info %}
        {{ var }}{{ access }} =
            {{ expand_object_id_type(command, member_access_pairs) }}.Get(
                {{ expand_command(command, member_access_pairs) }}
            );
    {% else %}
        ASSERT(false); // {{ command.name.CamelCase() }}
    {% endif %}
{%- endmacro %}

{% macro assign_return_value(var, access, command, member_access_pairs) -%}
    {% if "returns" in command.lpm_info %}
        if ({{ expand_return_object_type(command, member_access_pairs) }}.size()
            < {{ expand_return_object_limit(command, member_access_pairs) }}) {
            {{ var }}{{ access }} =
                {{ expand_return_object_type(command, member_access_pairs) }}.ReserveHandle();
        }
    {% else %}
        ASSERT(false); // {{ command.name.CamelCase() }}
    {% endif %}
{%- endmacro %}

{% macro assign_structure(var, access, command, member_access_pairs, is_simple) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set local_var = as_varLocalLPM(var, member) %}

    // start assign_structure
    {{ as_cType(member.type.name) }} {{ local_var }};
    memset(&{{ local_var }}, 0, sizeof(struct {{ as_cType(member.type.name) }}));

    {% for type_member in member.type.members %}
        {% set type_member_access = '.' + as_varName(type_member.name) %}
        {% do member_access_pairs.append((type_member, None)) %}
        {{ proto_to_wire_helper(local_var, type_member_access, command, member_access_pairs) }}
        {% do member_access_pairs.pop() %}
    {% endfor %}

    {% if is_simple %}
        {{ var }}{{ access }} = {{ local_var }};
    {% else %}
        {{ var }}{{ access }} = &{{ local_var }};
    {% endif %}
    // end assign_structure
{%- endmacro %}

{% macro assign_repeated(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set varlen = as_varSizeLPM(var, member) %}
    {% set array_var = as_varLocalArrayLPM(var, member) %}

    size_t {{ varlen }} = {{ expand_repeated_command_size(command, member_access_pairs) }};
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }}]
    );
    memset({{ array_var }}.get(), 0, {{ varlen }} * sizeof({{ as_cType(member.type.name) }}));

    for (size_t i = 0; i < {{ varlen }}; i++) {
        {% set tmp = member_access_pairs.pop() %}
        {% do member_access_pairs.append((tmp[0], 'i')) %}
        {% set array_var_access = as_indexedAccessLPM('i') %}

        {% if member.type in by_category["structure"] %}
            {{ assign_structure(array_var, array_var_access, command, member_access_pairs, True) }}
        {% elif member.type in by_category["bitmask"] %}
            {{ assign_bitmask(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type in by_category["enum"] %}
            {{ assign_enum(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type in by_category["object"] %}
            {{ assign_object(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type.name.CamelCase() == "ObjectId" %}
            {{ assign_object_id(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.is_return_value or member.type.name.CamelCase() == "ObjectHandle" %}
            {{ assign_return_value(array_var, array_var_access, command, member_access_pairs) }}
        {% endif %}
    }

    {{ var }}{{ access }} = {{ array_var }}.get();
    {{ var }}.{{ as_varName(member.length.name) }} = {{ varlen }};
{%- endmacro %}



{% macro proto_to_wire_simple_type(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.type in by_category["structure"] %}
        {{ assign_structure(var, access, command, member_access_pairs, True) }}
    {% elif member.type in by_category["bitmask"] %}
        {{ assign_bitmask(var, access, command, member_access_pairs) }}
    {% elif member.type in by_category["enum"] %}
        {{ assign_enum(var, access, command, member_access_pairs) }}
    {% elif member.type in by_category["object"] %}
        {{ assign_object(var, access, command, member_access_pairs) }}
    {% elif member.type.name.CamelCase() == "ObjectId" %}
        {{ assign_object_id(var, access, command, member_access_pairs) }}
    {% elif member.is_return_value or member.type.name.CamelCase() == "ObjectHandle" %}
        {{ assign_return_value(var, access, command, member_access_pairs) }}
    {% else %}
        {{ assign_generic_value(var, access, command, member_access_pairs) }}
    {% endif %}
{%- endmacro %}

{% macro proto_to_wire_complex_type(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.annotation == 'const*' and member.length == 'strlen' and member.type.name.get() == 'char' %}
        {{ assign_string(var, access, command, member_access_pairs) }}
    {% elif member.annotation == 'const*' and member.length == 'constant' and member.type.name.get() == 'float' %}
        {{ assign_float_array(var, access, command, member_access_pairs) }}
    {% elif member.annotation == 'const*' and member.length != 'constant' and member.type.name.get() == 'uint8_t' %}
        {{ assign_byte_array(var, access, command, member_access_pairs) }}
    {% elif member.annotation == 'const*' and member.length == 'constant'%}
        {{ assign_structure(var, access, command, member_access_pairs, False) }}
    {% elif member.annotation == 'const*' and member.length != 'strlen' and member.length != 'constant' %}
        {{ assign_repeated(var, access, command, member_access_pairs) }}
    {% elif member.type.name.get() == "void" %}
    {% else %}
        ASSERT(false);  // proto_to_wire_complex_type {{ command.name.CamelCase() }}
    {% endif %}
{%- endmacro %}

{% macro proto_to_wire_helper(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.skip_serialize == True %}
    {% elif "override" in member.lpm_info %}
        {{ assign_override(var, access, member.lpm_info["override"]) }}
    {% elif member.annotation == 'value' %}
        {{ proto_to_wire_simple_type(var, access, command, member_access_pairs) }}
    {% else %}
        {{ proto_to_wire_complex_type(var, access, command, member_access_pairs) }}
    {% endif %}
{%- endmacro %}


namespace dawn::wire {

void SerializedData(const fuzzing::Program& program, dawn::wire::ChunkedCommandSerializer serializer) {
    DawnLPMObjectIdProvider provider;
    ityp::array<ObjectType, client::DawnLPMObjectStore, 24> gObjectStores;

    for (const fuzzing::Command& command : program.commands()) {
        switch (command.command_case()) {

            {% for command in cmd_records["cpp_commands"] %}
            case fuzzing::Command::k{{command.name.CamelCase()}}: {
                {{ command.name.CamelCase() }}Cmd {{ 'cmd' }};
                // TODO(chromium:1374747): Populate command buffer with serialized code from generated
                // protobuf structures. Currently, this will nullptr-deref constantly.
                memset(&{{ 'cmd' }}, 0, sizeof({{ command.name.CamelCase() }}Cmd));
                {% for member in command.members %}
                    {% set member_access = as_memberAccessLPM(as_varName(member.name)) %}
                    {% set member_access_pairs = [(member, None)] %}

                    {{ proto_to_wire_helper('cmd', member_access, command, member_access_pairs ) }}
                {% endfor %}
                serializer.SerializeCommand(cmd, provider);
                break;
            }
            {% endfor %}
            default: {
                GetCustomSerializedData(command, serializer, gObjectStores, provider);
                break;
            }
        }
    }
}

} // namespace dawn::wire
