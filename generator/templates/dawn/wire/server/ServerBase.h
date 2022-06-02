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
#ifndef {{DIR}}_SERVER_SERVERBASE_H_
#define {{DIR}}_SERVER_SERVERBASE_H_

{% set api = metadata.api.lower() %}
#include "dawn/{{api}}_proc_table.h"
{% set wire_dir = namespace_name.Dirs() %}
{% set wire_namespace = namespace_name.namespace_case() %}
#include "{{wire_dir}}/ChunkedCommandHandler.h"
#include "{{wire_dir}}/Wire.h"
#include "{{wire_dir}}/WireCmd_autogen.h"
#include "{{wire_dir}}/WireDeserializeAllocator.h"
#include "{{wire_dir}}/server/ObjectStorage.h"

namespace {{wire_namespace}}::server {

    class ServerBase : public ChunkedCommandHandler, public ObjectIdResolver {
      public:
        ServerBase() = default;
        virtual ~ServerBase() = default;

      protected:
        {% set prefix = metadata.proc_table_prefix %}
        void DestroyAllObjects(const {{prefix}}ProcTable& procs) {
            {% set c_prefix = metadata.c_prefix %}
            {% set variable = "Device" %}
            {% if c_prefix == "WNN" %}
                {% set variable = "Context" %}
            {% endif %} 
            //* Free all objects when the server is destroyed
            {% for type in by_category["object"] if type.name.get() != variable.lower() %}
                {
                    std::vector<{{as_cType(type.name)}}> handles = mKnown{{type.name.CamelCase()}}.AcquireAllHandles();
                    for ({{as_cType(type.name)}} handle : handles) {
                        procs.{{as_varName(type.name, Name("release"))}}(handle);
                    }
                }
            {% endfor %}

            {   
                std::vector<{{c_prefix}}{{variable}}> handles = mKnown{{variable}}.AcquireAllHandles();
                for ({{c_prefix}}{{variable}} handle : handles) {
                    procs.{{variable.lower()}}Release(handle);
                }
            }
        }

        {% for type in by_category["object"] %}
            const KnownObjects<{{as_cType(type.name)}}>& {{type.name.CamelCase()}}Objects() const {
                return mKnown{{type.name.CamelCase()}};
            }
            KnownObjects<{{as_cType(type.name)}}>& {{type.name.CamelCase()}}Objects() {
                return mKnown{{type.name.CamelCase()}};
            }
        {% endfor %}

        {% for type in by_category["object"] if type.name.CamelCase() in server_reverse_lookup_objects %}
            const ObjectIdLookupTable<{{as_cType(type.name)}}>& {{type.name.CamelCase()}}ObjectIdTable() const {
                return m{{type.name.CamelCase()}}IdTable;
            }
            ObjectIdLookupTable<{{as_cType(type.name)}}>& {{type.name.CamelCase()}}ObjectIdTable() {
                return m{{type.name.CamelCase()}}IdTable;
            }
        {% endfor %}

      private:
        // Implementation of the ObjectIdResolver interface
        {% for type in by_category["object"] %}
            WireResult GetFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                auto data = mKnown{{type.name.CamelCase()}}.Get(id);
                if (data == nullptr) {
                    return WireResult::FatalError;
                }

                *out = data->handle;
                return WireResult::Success;
            }

            WireResult GetOptionalFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                if (id == 0) {
                    *out = nullptr;
                    return WireResult::Success;
                }

                return GetFromId(id, out);
            }
        {% endfor %}

        //* The list of known IDs for each object type.
        {% for type in by_category["object"] %}
            KnownObjects<{{as_cType(type.name)}}> mKnown{{type.name.CamelCase()}};
        {% endfor %}

        {% for type in by_category["object"] if type.name.CamelCase() in server_reverse_lookup_objects %}
            ObjectIdLookupTable<{{as_cType(type.name)}}> m{{type.name.CamelCase()}}IdTable;
        {% endfor %}
    };

}  // namespace {{wire_namespace}}::server

#endif  // {{DIR}}_SERVER_SERVERBASE_H_
