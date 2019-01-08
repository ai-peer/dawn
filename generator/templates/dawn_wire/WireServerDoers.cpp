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
        //* Implementation of the command doers
        {% for command in by_category["command"] %}
            {% set type = command.derived_object %}
            {% set method = command.derived_method %}
            {% set is_method = method != None %}
            {% set returns = is_method and method.return_type.name.canonical_case() != "void" %}
            {% set return_type = method.return_type if returns else None %}

            {% set Suffix = command.name.CamelCase() %}
            {% if Suffix not in client_side_commands %}
                {% if is_method and Suffix not in server_custom_commands %}
                    bool WireServer::Do{{Suffix}}({{as_cType(type.name)}} cSelf
                        {%- for arg in method.arguments -%}
                            , {{as_annotated_cType(arg)}}
                        {%- endfor -%}
                        {%- if returns -%}
                            , {{as_cType(method.return_type.name)}}* result
                        {%- endif -%}
                    ) {
                        {% if returns %}
                            *result =
                        {% endif %}
                        mProcs.{{as_varName(type.name, method.name)}}(cSelf
                            {%- for arg in method.arguments -%}
                                , {{as_varName(arg.name)}}
                            {%- endfor -%}
                        );
                        return true;
                    }
                {% endif %}
            {% endif %}
        {% endfor %}

        bool WireServer::DoDestroyObject(ObjectType objectType, ObjectId objectId) {
            //* ID 0 are reserved for nullptr and cannot be destroyed.
            if (objectId == 0) {
                return false;
            }

            switch(objectType) {
                {% for type in by_category["object"] %}
                    case ObjectType::{{type.name.CamelCase()}}: {
                        {% if type.name.CamelCase() == "Device" %}
                            //* Freeing the device has to be done out of band.
                            return false;
                        {% else %}
                            auto* data = {{type.name.CamelCase()}}Objects().Get(objectId);
                            if (data == nullptr) {
                                return false;
                            }
                            {% if type.name.CamelCase() in server_reverse_lookup_objects %}
                                {{type.name.CamelCase()}}ObjectIdTable().Remove(data->handle);
                            {% endif %}
                            if (data->handle != nullptr) {
                                mProcs.{{as_varName(type.name, Name("release"))}}(data->handle);
                            }
                            {{type.name.CamelCase()}}Objects().Free(objectId);
                            return true;
                        {% endif %}
                    }
                {% endfor %}
                default:
                    UNREACHABLE();
                    return false;
            }

            return true;
        }
    }
}
