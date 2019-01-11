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

#include "dawn_wire/client/WireClient.h"

namespace dawn_wire {
    namespace client {
        //* Implementation of the client API functions.
        {% for type in by_category["object"] %}
            {% set Type = type.name.CamelCase() %}
            {% set cType = as_cType(type.name) %}

            {% for method in type.methods + type.native_methods %}
                {% set Suffix = as_MethodSuffix(type.name, method.name) %}
                {{as_cType(method.return_type.name)}} Client{{Suffix}}(
                    {{-cType}} cSelf
                    {%- for arg in method.arguments -%}
                        , {{as_annotated_cType(arg)}}
                    {%- endfor -%}
                ) {
                    {{as_wireType(type)}} self = reinterpret_cast<{{as_wireType(type)}}>(cSelf);
                    return self->client->{{Suffix}}(
                      cSelf
                      {%- for arg in method.arguments -%}
                        , {{as_varName(arg.name)}}
                      {%- endfor -%}
                    );
                }
            {% endfor %}

            {% if type.is_builder %}
                void Client{{as_MethodSuffix(type.name, Name("set error callback"))}}(
                    {{cType}} cSelf,
                    dawnBuilderErrorCallback callback,
                    dawnCallbackUserdata userdata1,
                    dawnCallbackUserdata userdata2) {
                    {{as_wireType(type)}} self = reinterpret_cast<{{as_wireType(type)}}>(cSelf);
                    return self->client->{{as_MethodSuffix(type.name, Name("set error callback"))}}(
                      cSelf, callback, userdata1, userdata2
                    );
                }
            {% endif %}

            //* When an object's refcount reaches 0, notify the server side of it and delete it.
            void Client{{as_MethodSuffix(type.name, Name("release"))}}({{cType}} cObj) {
                {{as_wireType(type)}} obj = reinterpret_cast<{{as_wireType(type)}}>(cObj);
                return obj->client->{{as_MethodSuffix(type.name, Name("release"))}}(cObj);
            }

            void Client{{as_MethodSuffix(type.name, Name("reference"))}}({{cType}} cObj) {
                {{as_wireType(type)}} obj = reinterpret_cast<{{as_wireType(type)}}>(cObj);
                return obj->client->{{as_MethodSuffix(type.name, Name("reference"))}}(cObj);
            }
        {% endfor %}

        dawnProcTable GetProcs() {
            dawnProcTable table;
            {% for type in by_category["object"] %}
                {% for method in native_methods(type) %}
                    {% set suffix = as_MethodSuffix(type.name, method.name) %}
                    table.{{as_varName(type.name, method.name)}} = Client{{suffix}};
                {% endfor %}
            {% endfor %}
            return table;
        }

        {% for type in by_category["object"] %}
            {% set Type = type.name.CamelCase() %}
            {% set cType = as_cType(type.name) %}

            {% for method in type.methods %}
                {% set Suffix = as_MethodSuffix(type.name, method.name) %}
                {% if Suffix not in client_custom_commands %}
                    {{as_cType(method.return_type.name)}} WireClient::{{Suffix}}(
                        {{-cType}} cSelf
                        {%- for arg in method.arguments -%}
                            , {{as_annotated_cType(arg)}}
                        {%- endfor -%}
                    ) {
                        {{Suffix}}Cmd cmd;

                        //* Create the structure going on the wire on the stack and fill it with the value
                        //* arguments so it can compute its size.
                        cmd.self = cSelf;

                        //* For object creation, store the object ID the client will use for the result.
                        {% if method.return_type.category == "object" %}
                            auto* allocation = {{method.return_type.name.CamelCase()}}Allocator().New();

                            {% if type.is_builder %}
                                auto self = reinterpret_cast<{{as_wireType(type)}}>(cSelf);
                                //* We are in GetResult, so the callback that should be called is the
                                //* currently set one. Copy it over to the created object and prevent the
                                //* builder from calling the callback on destruction.
                                allocation->object->builderCallback = self->builderCallback;
                                self->builderCallback.canCall = false;
                            {% endif %}

                            cmd.result = allocation->GetHandle();
                        {% endif %}

                        {% for arg in method.arguments %}
                            cmd.{{as_varName(arg.name)}} = {{as_varName(arg.name)}};
                        {% endfor %}

                        //* Allocate space to send the command and copy the value args over.
                        size_t requiredSize = cmd.GetRequiredSize();
                        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                        cmd.Serialize(allocatedBuffer, *this);

                        {% if method.return_type.category == "object" %}
                            return reinterpret_cast<{{as_cType(method.return_type.name)}}>(allocation->object.get());
                        {% endif %}
                    }
                {% endif %}
            {% endfor %}

            {% if type.is_builder %}
                void WireClient::{{as_MethodSuffix(type.name, Name("set error callback"))}}({{cType}} cSelf,
                                                                                      dawnBuilderErrorCallback callback,
                                                                                      dawnCallbackUserdata userdata1,
                                                                                      dawnCallbackUserdata userdata2) {
                    {{Type}}* self = reinterpret_cast<{{Type}}*>(cSelf);
                    self->builderCallback.callback = callback;
                    self->builderCallback.userdata1 = userdata1;
                    self->builderCallback.userdata2 = userdata2;
                }
            {% endif %}

            {% if not type.name.canonical_case() == "device" %}
                //* When an object's refcount reaches 0, notify the server side of it and delete it.
                void WireClient::{{as_MethodSuffix(type.name, Name("release"))}}({{cType}} cObj) {
                    {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
                    obj->refcount--;

                    if (obj->refcount > 0) {
                        return;
                    }

                    obj->builderCallback.Call(DAWN_BUILDER_ERROR_STATUS_UNKNOWN, "Unknown");

                    DestroyObjectCmd cmd;
                    cmd.objectType = ObjectType::{{type.name.CamelCase()}};
                    cmd.objectId = obj->id;

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(obj->client->GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);

                    obj->client->{{type.name.CamelCase()}}Allocator().Free(obj);
                }

                void WireClient::{{as_MethodSuffix(type.name, Name("reference"))}}({{cType}} cObj) {
                    {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
                    obj->refcount++;
                }
            {% endif %}
        {% endfor %}
    }
}
