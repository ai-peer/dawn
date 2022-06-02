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
{% set namespace_name = Name(metadata.wire_namespace) %}
{% set DIR = namespace_name.concatcase().upper() %}
#ifndef {{DIR}}_CLIENT_APIOBJECTS_AUTOGEN_H_
#define {{DIR}}_CLIENT_APIOBJECTS_AUTOGEN_H_

{% set wire_dir = namespace_name.Dirs() %}
{% set wire_namespace = namespace_name.namespace_case() %}
#include "{{wire_dir}}/ObjectType_autogen.h"
#include "{{wire_dir}}/client/ObjectBase.h"

namespace {{wire_namespace}}::client {

    template <typename T>
    struct ObjectTypeToTypeEnum {
        static constexpr ObjectType value = static_cast<ObjectType>(-1);
    };

    {% for type in by_category["object"] %}
        {% set Type = type.name.CamelCase() %}
        {% if type.name.CamelCase() in client_special_objects %}
            class {{Type}};
        {% else %}
            struct {{type.name.CamelCase()}} final : ObjectBase {
                using ObjectBase::ObjectBase;
            };
        {% endif %}

        {% set PREFIX = metadata.c_prefix %}
        inline {{Type}}* FromAPI({{PREFIX}}{{Type}} obj) {
            return reinterpret_cast<{{Type}}*>(obj);
        }
        inline {{PREFIX}}{{Type}} ToAPI({{Type}}* obj) {
            return reinterpret_cast<{{PREFIX}}{{Type}}>(obj);
        }

        template <>
        struct ObjectTypeToTypeEnum<{{Type}}> {
            static constexpr ObjectType value = ObjectType::{{Type}};
        };

    {% endfor %}
}  // namespace {{wire_namespace}}::client

#endif  // {{DIR}}_CLIENT_APIOBJECTS_AUTOGEN_H_