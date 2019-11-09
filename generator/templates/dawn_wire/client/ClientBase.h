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

#ifndef DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_
#define DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_

#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/client/ApiObjects.h"
#include "dawn_wire/client/ObjectAllocator.h"

namespace dawn_wire { namespace client {

    {% for type in by_category["object"] %}
        constexpr ObjectType GetObjectType(const {{type.name.CamelCase()}}*){
            return ObjectType::{{type.name.CamelCase()}};
        }
    {% endfor %}

    constexpr uint32_t kObjectTypeCount = {{ len(by_category["object"]) }};

    class ClientBase : public ObjectIdProvider {
      public:
        ClientBase() :
        {% for type in by_category["object"] %}
           m{{type.name.CamelCase()}}Allocator(this){{ "," if not loop.last }}
        {% endfor %}
        {
        }

        virtual ~ClientBase() {
        }

        {% for type in by_category["object"] %}
            const ObjectAllocator<{{type.name.CamelCase()}}>& {{type.name.CamelCase()}}Allocator() const {
                return m{{type.name.CamelCase()}}Allocator;
            }
            ObjectAllocator<{{type.name.CamelCase()}}>& {{type.name.CamelCase()}}Allocator() {
                return m{{type.name.CamelCase()}}Allocator;
            }
        {% endfor %}

        bool AcquireNeedsDestroy(ObjectType type, uint32_t id) {
            switch (type) {
                {% for type in by_category["object"] %}
                    case ObjectType::{{type.name.CamelCase()}}:
                        return {{type.name.CamelCase()}}Allocator().AcquireNeedsDestroy(id);
                {% endfor %}
                    default:
                        UNREACHABLE();
                        return false;
            }
        }

      private:
        // Implementation of the ObjectIdProvider interface
        {% for type in by_category["object"] %}
            ObjectId GetId({{as_cType(type.name)}} object) const final {
                return object == nullptr ? 0 : reinterpret_cast<{{as_wireType(type)}}>(object)->id;
            }
            ObjectId GetOptionalId({{as_cType(type.name)}} object) const final {
                if (object == nullptr) {
                    return 0;
                }
                return GetId(object);
            }
        {% endfor %}

        {% for type in by_category["object"] %}
            ObjectAllocator<{{type.name.CamelCase()}}> m{{type.name.CamelCase()}}Allocator;
        {% endfor %}
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_
