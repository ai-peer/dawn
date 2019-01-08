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

#ifndef DAWNWIRE_CLIENT_WIRESERVERBASE_AUTOGEN_H_
#define DAWNWIRE_CLIENT_WIRESERVERBASE_AUTOGEN_H_

#include "dawn_wire/Wire.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/WireServerObjectStorage.h"

namespace dawn_wire {

    namespace server {

        class WireServerBase : public CommandHandler, public ObjectIdResolver {
            public:
                WireServerBase(const dawnProcTable& procs)
                    : mProcs(procs) {
                }

                ~WireServerBase() override {
                    //* Free all objects when the server is destroyed
                    {% for type in by_category["object"] if type.name.canonical_case() != "device" %}
                        for ({{as_cType(type.name)}} handle : mKnown{{type.name.CamelCase()}}.AcquireAllHandles()) {
                            mProcs.{{as_varName(type.name, Name("release"))}}(handle);
                        }
                    {% endfor %}
                }

            protected:
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

                //* Implementation of the ObjectIdResolver interface
                {% for type in by_category["object"] %}
                    DeserializeResult GetFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                        auto data = mKnown{{type.name.CamelCase()}}.Get(id);
                        if (data == nullptr) {
                            return DeserializeResult::FatalError;
                        }

                        *out = data->handle;
                        if (data->valid) {
                            return DeserializeResult::Success;
                        } else {
                            return DeserializeResult::ErrorObject;
                        }
                    }

                    DeserializeResult GetOptionalFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                        if (id == 0) {
                            *out = nullptr;
                            return DeserializeResult::Success;
                        }

                        return GetFromId(id, out);
                    }
                {% endfor %}

            private:
                //* The list of known IDs for each object type.
                {% for type in by_category["object"] %}
                    KnownObjects<{{as_cType(type.name)}}> mKnown{{type.name.CamelCase()}};
                {% endfor %}

                {% for type in by_category["object"] if type.name.CamelCase() in server_reverse_lookup_objects %}
                    ObjectIdLookupTable<{{as_cType(type.name)}}> m{{type.name.CamelCase()}}IdTable;
                {% endfor %}

                dawnProcTable mProcs;
        };
    }

}  // namespace dawn_wire

#endif  // DAWNWIRE_CLIENT_WIRESERVERBASE_AUTOGEN_H_
