//* Copyright 2019 The Dawn Authors
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

#include "dawn_wire/WireServer.h"

namespace dawn_wire {
    namespace server {
        //* Implementation of the command handlers
        {% for command in by_category["command"] %}
            {% set type = command.derived_object %}
            {% set method = command.derived_method %}
            {% set is_method = method != None %}
            {% set returns = is_method and method.return_type.name.canonical_case() != "void" %}
            {% set return_type = method.return_type if returns else None %}

            {% set Suffix = command.name.CamelCase() %}
            {% if Suffix not in client_side_commands %}
                //* The generic command handlers
                bool WireServer::Handle{{Suffix}}(const char** commands, size_t* size) {
                    {{Suffix}}Cmd cmd;
                    {% if serialization_info[command.dict_name].has_dawn_object %}
                        DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator, *this);
                    {% else %}
                        DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator);
                    {% endif %}

                    if (deserializeResult == DeserializeResult::FatalError) {
                        return false;
                    }

                    {% if Suffix in server_custom_pre_handler_commands %}
                        if (!PreHandle{{Suffix}}(cmd)) {
                            return false;
                        }
                    {% endif %}

                    {% if is_method %}
                        //* Unpack 'self'
                        auto* selfData = {{type.name.CamelCase()}}Objects().Get(cmd.selfId);
                        ASSERT(selfData != nullptr);
                    {% endif %}

                    //* Allocate any result objects
                    {%- for output in command.outputs -%}
                        {% if output.type.category == "object" and output.annotation == "handle" %}
                            {% set Type = output.type.name.CamelCase() %}
                            {% set name = as_varName(output.name) %}
                            auto* {{name}}Data = {{Type}}Objects().Allocate(cmd.{{name}}.id);
                            if ({{name}}Data == nullptr) {
                                return false;
                            }
                            {{name}}Data->serial = cmd.{{name}}.serial;

                            {% if type.is_builder %}
                                selfData->builtObjectId = cmd.{{name}}.id;
                                selfData->builtObjectSerial = cmd.{{name}}.serial;
                            {% endif %}
                        {% endif %}
                    {% endfor %}

                    //* After the data is allocated, apply the argument error propagation mechanism
                    if (deserializeResult == DeserializeResult::ErrorObject) {
                        {% if type.is_builder %}
                            selfData->valid = false;
                            //* If we are in GetResult, fake an error callback
                            {% if returns %}
                                On{{type.name.CamelCase()}}Error(DAWN_BUILDER_ERROR_STATUS_ERROR, "Maybe monad", cmd.selfId, selfData->serial);
                            {% endif %}
                        {% endif %}
                        return true;
                    }

                    //* Do command
                    bool success = Do{{Suffix}}(
                        {%- for input in command.inputs -%}
                            cmd.{{as_varName(input.name)}}
                            {%- if len(command.outputs) or not loop.last -%}, {% endif %}
                        {%- endfor -%}
                        {% for output in command.outputs %}
                            //* Pass the handle of the output object to be written by the doer
                            {%- if output.type.category == "object" and output.annotation == "handle" -%}
                                &{{as_varName(output.name)}}Data->handle
                            {%- else -%}
                                &cmd.{{as_varName(output.name)}}
                            {%- endif -%}
                            {%- if not loop.last -%}, {% endif %}
                        {%- endfor -%}
                    );

                    //* Mark output object handles as valid/invalid
                    {% for output in command.outputs %}
                        {% if output.type.category == "object" and output.annotation == "handle" %}
                            {% set name = as_varName(output.name) %}
                            {{name}}Data->valid = {{name}}Data->handle != nullptr;
                        {% endif %}
                    {% endfor %}

                    if (!success) {
                        return false;
                    }

                    {% if Suffix in server_custom_post_handler_commands %}
                        if (!PostHandle{{Suffix}}(cmd)) {
                            return false;
                        }
                    {% endif %}

                    {%- for output in command.outputs -%}
                        {% if output.type.category == "object" and output.annotation == "handle" %}
                            {% set name = as_varName(output.name) %}

                            {% if output.type.name.CamelCase() in server_reverse_lookup_objects %}
                                //* For created objects, store a mapping from them back to their client IDs
                                if ({{name}}Data->valid) {
                                    {{output.type.name.CamelCase()}}ObjectIdTable().Store({{name}}Data->handle, cmd.{{name}}.id);
                                }
                            {% endif %}

                            //* builders remember the ID of the object they built so that they can send it
                            //* in the callback to the client.
                            {% if output.type.is_builder %}
                                if ({{name}}Data->valid) {
                                    uint64_t userdata1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
                                    uint64_t userdata2 = (uint64_t({{name}}Data->serial) << uint64_t(32)) + cmd.{{name}}.id;
                                    mProcs.{{as_varName(output.type.name, Name("set error callback"))}}({{name}}Data->handle, Forward{{output.type.name.CamelCase()}}ToClient, userdata1, userdata2);
                                }
                            {% endif %}
                        {% endif %}
                    {% endfor %}

                    return true;
                }
            {% endif %}
        {% endfor %}

        const char* WireServer::HandleCommands(const char* commands, size_t size) {
            mProcs.deviceTick(DeviceObjects().Get(1)->handle);
            while (size >= sizeof(WireCmd)) {
                WireCmd cmdId = *reinterpret_cast<const WireCmd*>(commands);

                bool success = false;
                switch (cmdId) {
                    {% for command in by_category["command"] %}
                        {% set Suffix = command.name.CamelCase() %}
                        {% if Suffix not in client_side_commands %}
                            case WireCmd::{{Suffix}}:
                                success = Handle{{Suffix}}(&commands, &size);
                                break;
                        {% endif %}
                    {% endfor %}
                    default:
                        success = false;
                }

                if (!success) {
                    return nullptr;
                }
                mAllocator.Reset();
            }

            if (size != 0) {
                return nullptr;
            }

            return commands;
        }
    }
}
