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
#include "dawn/wire/ObjectHandle.h"

/*
 * General Structure: Types are taken from dawn.json and dawn_wire.json. The
 * CmdState object holds all the state needed for generating the protobuf
 * calls.
 *
 * Using CmdState's stack, this generator recursively walks up each type:
 * structures, bitmasks, enums, objects, objectids, objecthandles, fixed-length
 * bytearrays, fixed-length float arrays, strings, and variable length arrays.
 *
 * The CmdState stack holds metadata about how to access the given protobuf
 * value by both storing the member name and index to access.
 * protobuf_access_helper is used generate a protobuf "chained" access like
 * `command.devicecreatebindgroup().desc().entries(i).binding()` from a
 * CmdState("device create bind group") with a stack that looks like
 * [LPMIR('desc', None), LPMIR('entries', 'i'), LPMIR('binding', None)]
 *
 * lift_dawn_ prefixed functions translate information from dawn.json and
 * dawn_wire.json into C++.
 *
 * emit_ prefixed functions are helpers that are building blocks called by
 * lift functions that translate a given type or access into C++ from
 * dawn.json.
 *
 * lift_dawn_member_pass_by_value: Writes dawn.json objects that are passed to
 * the serializer by value. Allocate the objects and fill them with values from
 * the libprotobuf-mutator bytestream.
 *
 * lift_dawn_member_pass_by_pointer: Writes dawn.json objects that are passed to
 * the serializer by pointer. Allocate the objects and fill them with
 * values from the libprotobuf-mutator bytestream.
 *
 * emit_protobuf_member_access: converts the type information in dawn.json and
 * dawn_wire.json into an lpm access.
 *
*/

{% macro protobuf_access_helper(state) -%}
    {%- for ir in state -%}
        .{{- ir.member_name }}({{ ir.protobuf_access }})
    {%- endfor -%}
{%- endmacro %}

{% macro emit_protobuf_member_access(state) -%}
    command.{{ state.protobuf_command_name }}(){{ protobuf_access_helper(state) }}
{%- endmacro %}

{% macro emit_protobuf_varlength_size(state) -%}
    command.{{ state.protobuf_command_name }}(){{ protobuf_access_helper(state) }}.size()
{%- endmacro %}

{% macro emit_protobuf_to_bitmask(state) -%}
    {% set member = state.top().member %}
    static_cast<{{ as_cType(member.type.name) }}>(
        {{ emit_protobuf_member_access(state) }}
    )
{%- endmacro %}

{% macro emit_protobuf_to_enum(state) -%}
    {% set member = state.top().member %}

    static_cast<{{ as_cType(member.type.name) }}>(
        {{ emit_protobuf_member_access(state) }}
    )
{%- endmacro %}

{% macro emit_protobuf_to_objecttype(state) -%}
    {% set member = state.top().member %}

    gObjectStores[ObjectType::{{ member.type.name.CamelCase() }}]
{%- endmacro %}

{% macro emit_protobuf_to_objectid(state) -%}
    {% set member = state.top().member %}

    gObjectStores[ObjectType::{{ member.id_type.name.CamelCase() }}]
{%- endmacro %}

{% macro emit_protobuf_to_objecthandle(state) -%}
    {% set member = state.top().member %}

    gObjectStores[ObjectType::{{ member.handle_type.name.CamelCase() }}]
{%- endmacro %}

{% macro emit_objecthandle_limit(state) -%}
    {% set member = state.top().member %}

    DawnLPMFuzzer::k{{ member.handle_type.name.CamelCase() }}Limit
{%- endmacro %}

/*
 * We are currently returning "main" for all strings because arbitrary strings
 * don't lead to more coverage in most Dawn APIs (currently). However, having
 * a valid entrypoint for compute pipeline execution leads to a lot of coverage.
 *
 * TODO(1374747): Add "entrypoint" to the list of overrides in dawn_lpm.json
 * and return arbitrary strings here.
*/
{% macro emit_protobuf_to_string(state) -%}
    "main"
{% endmacro %}

// Lifting functions
{% macro lift_lpm_override(state, cpp_var, cpp_access, overrides) %}
    {{ cpp_var }}{{ cpp_access }} =
        {{ overrides[state.overrides_key][state.top().overrides_key] }};
{% endmacro %}

{% macro lift_dawn_bitmask(state, cpp_var, cpp_access) -%}
    {% set bmlen_tempvar = state.protobuf_command_name + '_bmlen' %}

    for (size_t bm = 0; bm < static_cast<size_t>({{ emit_protobuf_varlength_size(state) }}); bm++) {
        {% do state.insert_top_access('bm') %}

        {{ cpp_var }}{{ cpp_access }} |= {{ emit_protobuf_to_bitmask(state) }};
    }
{%- endmacro %}

{% macro lift_dawn_enum(state, cpp_var, cpp_access) -%}
    {{ cpp_var }}{{ cpp_access }} = {{ emit_protobuf_to_enum(state) }};
{%- endmacro %}

{% macro lift_dawn_object(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}

    {{ cpp_var }}{{ cpp_access }} = reinterpret_cast<{{ as_cType(member.type.name) }}>(
        {{ emit_protobuf_to_objecttype(state) }}.Get(
             static_cast<ObjectId>(
                {{ emit_protobuf_member_access(state) }}
             )
        )
    );
{%- endmacro %}

{% macro lift_dawn_objectid(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}

    {{ cpp_var }}{{ cpp_access }} =
        {{ emit_protobuf_to_objectid(state) }}.Get(
             static_cast<ObjectId>(
                {{ emit_protobuf_member_access(state) }}
             )
        );
{%- endmacro %}

{% macro lift_dawn_objecthandle(state, cpp_var, cpp_member) -%}
    if ({{ emit_protobuf_to_objecthandle(state) }}.size()
        < {{ emit_objecthandle_limit(state) }}) {
        {{ cpp_var }}{{ cpp_member }} =
            {{ emit_protobuf_to_objecthandle(state) }}.ReserveHandle();
    }
{%- endmacro %}

{% macro lift_dawn_primitive(state, cpp_var, cpp_access) -%}
    {{ cpp_var }}{{ cpp_access  }} =
        {{ emit_protobuf_member_access(state) }};
{%- endmacro %}

{% macro lift_dawn_structure(state, cpp_var, cpp_access, is_pass_by_value) -%}
    {% set member = state.top().member %}
    {% set local_var = as_varLocalLPM(cpp_var, member) %}

    // Structure
    {{ as_cType(member.type.name) }} {{ local_var }};
    memset(&{{ local_var }}, 0, sizeof(struct {{ as_cType(member.type.name) }}));

    {% for type_member in member.type.members %}
        {% do state.push(type_member, None) %}
        {{ proto_to_wire_helper(state, local_var, as_memberAccess(type_member.name.camelCase())) }}
        {% do state.pop() %}
    {% endfor %}

    {% if is_pass_by_value %}
        {{ cpp_var }}{{ cpp_access }} = {{ local_var }};
    {% else %}
        {{ cpp_var }}{{ cpp_access }} = &{{ local_var }};
    {% endif %}
{%- endmacro %}

{% macro lift_dawn_string(state, cpp_var, cpp_access) -%}
    {{ cpp_var }}{{ cpp_access }} = {{ emit_protobuf_to_string(state) }};
{%- endmacro %}


{% macro lift_dawn_float_array(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}
    {% set array_var = as_varLocalArrayLPM(cpp_var, member) %}
    {% set varlen = as_varSizeLPM(cpp_var, member) %}

    size_t {{ varlen }} = 1024;
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }} ]
    );
    memset({{ array_var }}.get(), 0x41, {{ varlen }} * sizeof({{ as_cType(member.type.name) }}));

    {{ cpp_var }}{{ cpp_access }} = {{ array_var }}.get();
{%- endmacro %}

{% macro lift_dawn_byte_array(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}
    {% set array_var = as_varLocalArrayLPM(cpp_var, member) %}
    {% set varlen = as_varSizeLPM(cpp_var, member) %}

    size_t {{ varlen }} = 1024;
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }} ]
    );
    memset({{ array_var }}.get(), 0x41, {{ varlen }});

    {{ cpp_var }}{{ cpp_access }} = {{ array_var }}.get();
    {{ cpp_var }}.{{ as_varName(member.length.name) }} = {{ varlen }};
{%- endmacro %}

{% macro lift_dawn_varlength_array(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}
    {% set varlen = as_varSizeLPM(cpp_var, member) %}
    {% set array_var = as_varLocalArrayLPM(cpp_var, member) %}

    size_t {{ varlen }} = {{ emit_protobuf_varlength_size(state) }};
    std::unique_ptr<{{ as_cType(member.type.name) }}[]> {{ array_var }}(
        new {{ as_cType(member.type.name) }}[{{ varlen }}]
    );
    memset({{ array_var }}.get(), 0, {{ varlen }} * sizeof({{ as_cType(member.type.name) }}));

    for (size_t i = 0; i < {{ varlen }}; i++) {
        {% do state.insert_top_access('i') %}

        {% if member.type in by_category["structure"] %}
            {{ lift_dawn_structure(state, array_var, as_indexedAccess('i'), True) }}
        {% elif member.type in by_category["bitmask"] %}
            {{ lift_dawn_bitmask(state, array_var, as_indexedAccess('i')) }}
        {% elif member.type in by_category["enum"] %}
            {{ lift_dawn_enum(state, array_var, as_indexedAccess('i')) }}
        {% elif member.type in by_category["object"] %}
            {{ lift_dawn_object(state, array_var, as_indexedAccess('i')) }}
        {% elif member.type.name.get() == "ObjectId" %}
            {{ lift_dawn_objectid(state, array_var, as_indexedAccess('i')) }}
        {% elif member.type.name.get() == "ObjectHandle" %}
            {{ lift_dawn_objecthandle(state, array_var, as_indexedAccess('i')) }}
        {% endif %}
    }

    {{ cpp_var }}{{ cpp_access }} = {{ array_var }}.get();
    {{ cpp_var }}.{{ member.length.name.camelCase() }} = {{ varlen }};
{%- endmacro %}

{% macro lift_dawn_member_pass_by_value(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}

    {% if member.type in by_category["structure"] %}
        {{ lift_dawn_structure(state, cpp_var, cpp_access, True) }}
    {% elif member.type in by_category["bitmask"] %}
        {{ lift_dawn_bitmask(state, cpp_var, cpp_access) }}
    {% elif member.type in by_category["enum"] %}
        {{ lift_dawn_enum(state, cpp_var, cpp_access) }}
    {% elif member.type in by_category["object"] %}
        {{ lift_dawn_object(state, cpp_var, cpp_access) }}
    {% elif member.type.name.get() == "ObjectId" %}
        {{ lift_dawn_objectid(state, cpp_var, cpp_access) }}
    {% elif member.type.name.get() == "ObjectHandle" %}
        {{ lift_dawn_objecthandle(state, cpp_var, cpp_access) }}
    {% else %}
        {{ lift_dawn_primitive(state, cpp_var, cpp_access) }}
    {% endif %}
{%- endmacro %}

{% macro lift_dawn_member_pass_by_pointer(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}

    {% if member.type in by_category["structure"] and member.length == 'constant' %}
        {{ lift_dawn_structure(state, cpp_var, cpp_access, False) }}
    {% elif member.type.name.get() == 'char' and member.length == 'strlen' %}
        {{ lift_dawn_string(state, cpp_var, cpp_access) }}
    {% elif member.type.name.get() == 'float' %}
        {{ lift_dawn_float_array(state, cpp_var, cpp_access) }}
    {% elif member.type.name.get() == 'uint8_t' %}
        {{ lift_dawn_byte_array(state, cpp_var, cpp_access) }}
    {% elif member.length != 'strlen' and member.length != 'constant' %}
        {{ lift_dawn_varlength_array(state, cpp_var, cpp_access) }}
    {% else %}
        // If we are missing any new WebGPU types then we'd like to know at compile time
        // as it will likely cause uninteresting segfaults while fuzzing.
        {{ unreachable_code() }}
    {% endif %}
{%- endmacro %}


{% macro proto_to_wire_helper(state, cpp_var, cpp_access) -%}
    {% set member = state.top().member %}
    {% set overrides = cmd_records["lpm_info"]["overrides"]  %}

    {% if member.skip_serialize == True %}
    {% elif state.overrides_key in overrides and state.top().overrides_key in overrides[state.overrides_key] %}
        {{ lift_lpm_override(state, cpp_var, cpp_access, overrides) }}
    {% elif member.annotation == 'value' %}
        {{ lift_dawn_member_pass_by_value(state, cpp_var, cpp_access) }}
    {% elif member.annotation == 'const*' %}
        {{ lift_dawn_member_pass_by_pointer(state, cpp_var, cpp_access) }}
    {% else %}
        // If we are missing any new WebGPU types then we'd like to know at compile time
        // as it will likely cause uninteresting segfaults while fuzzing.
        {{ unreachable_code() }}
    {% endif %}
{%- endmacro %}

namespace dawn::wire {

void SerializedData(const fuzzing::Program& program, dawn::wire::ChunkedCommandSerializer serializer) {
    DawnLPMObjectIdProvider provider;
    ityp::array<ObjectType, DawnLPMObjectStore, {{len(by_category["object"])}}> gObjectStores;

    for (const fuzzing::Command& command : program.commands()) {
        switch (command.command_case()) {

            {% for command in cmd_records["cpp_generated_commands"] %}
            case fuzzing::Command::k{{command.name.CamelCase()}}: {
                {{ command.name.CamelCase() }}Cmd cmd;

                memset(&cmd, 0, sizeof({{ command.name.CamelCase() }}Cmd));
                {% for member in command.members %}
                    {% set state = CmdState(command, member) %}

                    {{ proto_to_wire_helper(state, 'cmd', as_memberAccess(member.name.camelCase())) }}
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
