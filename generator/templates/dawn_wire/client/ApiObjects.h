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

#ifndef DAWNWIRE_CLIENT_APIOBJECTS_AUTOGEN_H_
#define DAWNWIRE_CLIENT_APIOBJECTS_AUTOGEN_H_

#include "common/RefCounted.h"
#include "dawn_wire/ObjectType_autogen.h"
#include "dawn_wire/client/ObjectBase.h"

namespace dawn_wire { namespace client {

    template <typename T>
    struct ObjectTypeToTypeEnum {
        static constexpr ObjectType value = static_cast<ObjectType>(-1);
    };

    {% for type in by_category["object"] %}
        {% set Type = type.name.CamelCase() %}
        {% if Type in client_special_objects %}
            class {{Type}};
        {% else %}
            struct {{Type}} final : ObjectBase {
                using ObjectBase::ObjectBase;
            };
        {% endif %}

        inline {{Type}}* FromAPI(WGPU{{Type}} obj) {
            return reinterpret_cast<{{Type}}*>(obj);
        }
        inline WGPU{{Type}} ToAPI({{Type}}* obj) {
            return reinterpret_cast<WGPU{{Type}}>(obj);
        }

        template <>
        struct ObjectTypeToTypeEnum<{{Type}}> {
            static constexpr ObjectType value = ObjectType::{{Type}};
        };

        void Client{{as_MethodSuffix(type.name, Name("reference"))}}({{as_cType(type.name)}} cObj);
        void Client{{as_MethodSuffix(type.name, Name("release"))}}({{as_cType(type.name)}} cObj);

    {% endfor %}

}}  // namespace dawn_wire::client

//* Define Reference/Release for internal usages of Ref<T>
{% for type in by_category["object"] %}
    {% set Type = type.name.CamelCase() %}

    template <>
    struct RefCountedTraits<dawn_wire::client::{{Type}}> {
        static constexpr dawn_wire::client::{{Type}}* kNullValue = nullptr;
        static void Reference(dawn_wire::client::{{Type}}* value) {
            dawn_wire::client::Client{{as_MethodSuffix(type.name, Name("reference"))}}(ToAPI(value));
        }
        static void Release(dawn_wire::client::{{Type}}* value) {
            dawn_wire::client::Client{{as_MethodSuffix(type.name, Name("release"))}}(ToAPI(value));
        }
    };
{% endfor %}

#endif  // DAWNWIRE_CLIENT_APIOBJECTS_AUTOGEN_H_
