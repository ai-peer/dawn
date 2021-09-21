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

#ifndef DAWNNATIVE_APIOBJECTS_AUTOGEN_H_
#define DAWNNATIVE_APIOBJECTS_AUTOGEN_H_

#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/ObjectBase.h"

namespace dawn_native {

    template <typename T>
    struct ObjectTypeToTypeEnum {
        static constexpr ObjectType value = static_cast<ObjectType>(-1);
    };

    {% for type in by_category["object"] %}
        {% set Type = type.name.CamelCase() %}
        class {{Type}};

        template <>
        struct ObjectTypeToTypeEnum<{{Type}}> {
            static constexpr ObjectType value = ObjectType::{{Type}};
        };

    {% endfor %}
}  // namespace dawn_native

#endif  // DAWNNATIVE_APIOBJECTS_AUTOGEN_H_
