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

#include "dawn/fuzzers/lpmfuzz/DawnLPMConstants_autogen.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializer_autogen.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializerCustom.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMFuzzer.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMObjectStore.h"
#include "dawn/wire/Wire.h"
#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/ApiObjects_autogen.h"
#include "dawn/webgpu.h"
#include "dawn/wire/client/Client.h"

/*

lift_dawn_member_pass_by_value: Writes dawn.json objects that are passed to the
    serializer by value. Allocate the objects and fill them with values from the
    libprotobuf-mutator bytestream.

lift_dawn_member_pass_by_reference: Writes dawn.json objects that are passed to the
    serializer by reference. Allocate the objects and fill them with values from the
    libprotobuf-mutator bytestream.
*/

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

{% macro expand_string(command, member_access_pairs) -%}
    "main"
{%- endmacro %}

{% macro expand_return_object_type(command, member) -%}
    gObjectStores[ObjectType::{{ member.handle_type.name.CamelCase() }}]
{%- endmacro %}

{% macro expand_return_object_limit(command, member) -%}
    {{ member.handle_type.name.SNAKE_CASE() }}_LIMIT
{%- endmacro %}

{% macro expand_object_type(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}

    gObjectStores[ObjectType::{{ member.type.name.CamelCase() }}]
{%- endmacro %}

{% macro expand_object_id_type(command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}

    gObjectStores[ObjectType::{{ member.id_type.name.CamelCase() }}]
{%- endmacro %}


{% macro assign_override(var, access, override) %}
    {{ var }}{{ access }} = {{ override }};
{% endmacro %}

{% macro lift_dawn_enum(var, access, command, member_access_pairs) -%}
    {{ var }}{{ access }} = {{ expand_enum(command, member_access_pairs) }};
{%- endmacro %}

{% macro lift_dawn_primitive(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {{ var }}{{ access }} = {{ expand_command(command, member_access_pairs) }};
{%- endmacro %}

{% macro lift_dawn_string(var, access, command, member_access_pairs) -%}
    {{ var }}{{ access }} = {{ expand_string(command, member_access_pairs) }};
{%- endmacro %}

{% macro lift_dawn_float_array(var, access, command, member_access_pairs) -%}
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

{% macro lift_dawn_byte_array(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set array_var = as_varLocalArrayLPM(var, member) %}
    {% set varlen = as_varSizeLPM(var, member) %}

    size_t {{ varlen }} = 1024;
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }} ]
    );
    memset({{ array_var }}.get(), 0x41, {{ varlen }});

    {{ var }}{{ access }} = {{ array_var }}.get();
    {{ var }}.{{ as_varName(member.length.name) }} = {{ varlen }};
{%- endmacro %}

{% macro lift_dawn_bitmask(var, access, command, member_access_pairs) -%}
    {% set bmlen_tempvar = command.name.concatcase() + '_bmlen' %}

    size_t {{ bmlen_tempvar }} = {{ expand_repeated_command_size(command, member_access_pairs) }};
    for (size_t bm = 0; bm < {{ bmlen_tempvar }}; bm++) {
        {% set tmp = member_access_pairs.pop() %}
        {% do member_access_pairs.append((tmp[0], 'bm')) %}

        {{ var }}{{ access }} |= {{ expand_bitmask(command, member_access_pairs) }};
    }
{%- endmacro %}

{% macro lift_dawn_object(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}

    {{ var }}{{ access }} = reinterpret_cast<{{ as_cType(member.type.name) }}>(
        {{ expand_object_type(command, member_access_pairs) }}.Get(
                {{ expand_command(command, member_access_pairs) }}
            )
    );
{%- endmacro %}

{% macro lift_dawn_objectid(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    
    {{ var }}{{ access }} =
        {{ expand_object_id_type(command, member_access_pairs) }}.Get(
            {{ expand_command(command, member_access_pairs) }}
        );
{%- endmacro %}

{% macro lift_dawn_objecthandle(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}

    if ({{ expand_return_object_type(command, member) }}.size()
        < {{ expand_return_object_limit(command, member) }}) {
        {{ var }}{{ access }} =
            {{ expand_return_object_type(command, member) }}.ReserveHandle();
    }
{%- endmacro %}

{% macro lift_dawn_structure(var, access, command, member_access_pairs, is_pass_by_value) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% set local_var = as_varLocalLPM(var, member) %}

    {{ as_cType(member.type.name) }} {{ local_var }};
    memset(&{{ local_var }}, 0, sizeof(struct {{ as_cType(member.type.name) }}));

    {% for type_member in member.type.members %}
        {% set type_member_access = '.' + as_varName(type_member.name) %}
        {% do member_access_pairs.append((type_member, None)) %}
        {{ proto_to_wire_helper(local_var, type_member_access, command, member_access_pairs) }}
        {% do member_access_pairs.pop() %}
    {% endfor %}

    {% if is_pass_by_value %}
        {{ var }}{{ access }} = {{ local_var }};
    {% else %}
        {{ var }}{{ access }} = &{{ local_var }};
    {% endif %}
{%- endmacro %}

{% macro lift_dawn_varlength_array(var, access, command, member_access_pairs) -%}
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
            {{ lift_dawn_structure(array_var, array_var_access, command, member_access_pairs, True) }}
        {% elif member.type in by_category["bitmask"] %}
            {{ lift_dawn_bitmask(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type in by_category["enum"] %}
            {{ lift_dawn_enum(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type in by_category["object"] %}
            {{ lift_dawn_object(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type.name.get() == "ObjectId" %}
            {{ lift_dawn_objectid(array_var, array_var_access, command, member_access_pairs) }}
        {% elif member.type.name.get() == "ObjectHandle" %}
            {{ lift_dawn_objecthandle(array_var, array_var_access, command, member_access_pairs) }}
        {% endif %}
    }

    {{ var }}{{ access }} = {{ array_var }}.get();
    {{ var }}.{{ as_varName(member.length.name) }} = {{ varlen }};
{%- endmacro %}


{% macro lift_dawn_member_pass_by_value(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.type in by_category["structure"] %}
        {{ lift_dawn_structure(var, access, command, member_access_pairs, True) }}
    {% elif member.type in by_category["bitmask"] %}
        {{ lift_dawn_bitmask(var, access, command, member_access_pairs) }}
    {% elif member.type in by_category["enum"] %}
        {{ lift_dawn_enum(var, access, command, member_access_pairs) }}
    {% elif member.type in by_category["object"] %}
        {{ lift_dawn_object(var, access, command, member_access_pairs) }}
    {% elif member.type.name.get() == "ObjectId" %}
        {{ lift_dawn_objectid(var, access, command, member_access_pairs) }}
    {% elif member.type.name.get() == "ObjectHandle" %}
        {{ lift_dawn_objecthandle(var, access, command, member_access_pairs) }}
    {% else %}
        {{ lift_dawn_primitive(var, access, command, member_access_pairs) }}
    {% endif %}
{%- endmacro %}

{% macro lift_dawn_member_pass_by_reference(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.type in by_category["structure"] and member.length == 'constant' %}
        {{ lift_dawn_structure(var, access, command, member_access_pairs, False) }}
    {% elif member.type.name.get() == 'char' %}
        {{ lift_dawn_string(var, access, command, member_access_pairs) }}
    {% elif member.type.name.get() == 'float' %}
        {{ lift_dawn_float_array(var, access, command, member_access_pairs) }}
    {% elif member.type.name.get() == 'uint8_t' %}
        {{ lift_dawn_byte_array(var, access, command, member_access_pairs) }}
    {% elif member.length != 'strlen' and member.length != 'constant' %}
        {{ lift_dawn_varlength_array(var, access, command, member_access_pairs) }}
    {% else %}
        ASSERT(false);  // lift_dawn_wire_heap_variables {{ command.name.CamelCase() }}
    {% endif %}
{%- endmacro %}

{% macro proto_to_wire_helper(var, access, command, member_access_pairs) -%}
    {% set member = member_access_pairs[-1][0] %}
    {% if member.skip_serialize == True %}
        // {{ command.name.CamelCase() }}.{{ member.name.camelCase()}}.skip_serialize
    {% elif command.name.canonical_case() in cmd_records["lpm_info"]["overrides"] and  member.name.canonical_case() in cmd_records["lpm_info"]["overrides"][command.name.canonical_case()] %}
        {{ assign_override(var, access, cmd_records["lpm_info"]["overrides"][command.name.canonical_case()][member.name.canonical_case()] ) }}
    {% elif member.annotation == 'value' %}
        {{ lift_dawn_member_pass_by_value(var, access, command, member_access_pairs) }}
    {% elif member.annotation == 'const*' %}
        {{ lift_dawn_member_pass_by_reference(var, access, command, member_access_pairs) }}
    {% else %}
        ASSERT(false);
    {% endif %}
{%- endmacro %}


namespace dawn::wire {

void SerializedData(const fuzzing::Program& program, dawn::wire::ChunkedCommandSerializer serializer) {
    DawnLPMObjectIdProvider provider;
    ityp::array<ObjectType, client::DawnLPMObjectStore, {{len(by_category["object"])}}> gObjectStores;

    for (const fuzzing::Command& command : program.commands()) {
        switch (command.command_case()) {

            {% for command in cmd_records["cpp_commands"] %}
            case fuzzing::Command::k{{command.name.CamelCase()}}: {
                {{ command.name.CamelCase() }}Cmd {{ 'cmd' }};

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
