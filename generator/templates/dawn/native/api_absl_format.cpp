//* Copyright 2021 The Dawn Authors
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

{% set impl_dir = metadata.impl_dir + "/" if metadata.impl_dir else "" %}
{% set namespace_name = Name(metadata.native_namespace) %}
{% set native_namespace = namespace_name.namespace_case() %}
{% set native_dir = impl_dir + namespace_name.Dirs() %}
{% set api = metadata.api.lower() %}
#include "{{native_dir}}/{{api}}_absl_format_autogen.h"

#include "{{native_dir}}/ObjectType_autogen.h"

namespace {{native_namespace}} {

    //
    // Descriptors
    //

    {% for type in by_category["structure"] %}
        {% for member in type.members %}
            {% if member.name.canonical_case() == "label" %}
                absl::FormatConvertResult<absl::FormatConversionCharSet::kString>
                AbslFormatConvert(const {{as_cppType(type.name)}}* value,
                                    const absl::FormatConversionSpec& spec,
                                    absl::FormatSink* s) {
                    if (value == nullptr) {
                        s->Append("[null]");
                        return {true};
                    }
                    s->Append("[{{as_cppType(type.name)}}");
                    if (value->label != nullptr) {
                        s->Append(absl::StrFormat(" \"%s\"", value->label));
                    }
                    s->Append("]");
                    return {true};
                }
            {% endif %}
        {% endfor %}
    {% endfor %}

    //
    // Serializables
    //
    using absl::ParsedFormat;

    {% for type in by_category["structure"] %}
        {% if type.serializable %}
            absl::FormatConvertResult<absl::FormatConversionCharSet::kString>
                AbslFormatConvert(const {{as_cppType(type.name)}}& value,
                                  const absl::FormatConversionSpec& spec,
                                  absl::FormatSink* s) {
                {% set members = [] %}
                {% set format = [] %}
                {% set altFormat = [] %}
                {% set template = [] %}
                {% for member in type.members %}
                    {% set memberName = member.name.camelCase() %}
                    {% do members.append("value." + memberName) %}
                    {% do format.append(memberName + ": %" + as_formatType(member)) %}
                    {% do altFormat.append(member.member_id ~ ":%#" + as_formatType(member)) %}
                    {% do template.append("'" + as_formatType(member) + "'") %}
                {% endfor %}
                if (!spec.has_alt_flag()) {
                    static const auto* const prettyFmt =
                        new ParsedFormat<{{template|join(",")}}>("{ {{format|join(", ")}} }");
                    s->Append(absl::StrFormat(*prettyFmt, {{members|join(", ")}}));
                } else {
                    static const auto* const condensedFmt =
                        new ParsedFormat<{{template|join(",")}}>("{""{{altFormat|join(",")}}""}");
                    s->Append(absl::StrFormat(*condensedFmt, {{members|join(", ")}}));
                }
                return {true};
            }
        {% endif %}
    {% endfor %}

}  // namespace {{native_namespace}}

{% set namespace = metadata.namespace %}
namespace {{namespace}} {

    //
    // Enums
    //

    {% for type in by_category["enum"] %}
        absl::FormatConvertResult<absl::FormatConversionCharSet::kString|absl::FormatConversionCharSet::kIntegral>
        AbslFormatConvert({{as_cppType(type.name)}} value,
                          const absl::FormatConversionSpec& spec,
                          absl::FormatSink* s) {
            if (spec.conversion_char() == absl::FormatConversionChar::s && !spec.has_alt_flag()) {
                s->Append("{{as_cppType(type.name)}}::");
                switch (value) {
                {% for value in type.values %}
                    case {{as_cppType(type.name)}}::{{as_cppEnum(value.name)}}:
                        s->Append("{{as_cppEnum(value.name)}}");
                        break;
                {% endfor %}
                    default:
                        return {false};
                }
            } else {
                s->Append(absl::StrFormat("%u", static_cast<typename std::underlying_type<{{as_cppType(type.name)}}>::type>(value)));
            }
            return {true};
        }
    {% endfor %}

    //
    // Bitmasks
    //

    {% for type in by_category["bitmask"] %}
        absl::FormatConvertResult<absl::FormatConversionCharSet::kString|absl::FormatConversionCharSet::kIntegral>
        AbslFormatConvert({{as_cppType(type.name)}} value,
                            const absl::FormatConversionSpec& spec,
                            absl::FormatSink* s) {
            if (spec.conversion_char() == absl::FormatConversionChar::s && !spec.has_alt_flag()) {
                s->Append("{{as_cppType(type.name)}}::");
                if (!static_cast<bool>(value)) {
                    {% for value in type.values if value.value == 0 %}
                        // 0 is often explicitly declared as None.
                        s->Append("{{as_cppEnum(value.name)}}");
                    {% else %}
                        s->Append(absl::StrFormat("{{as_cppType(type.name)}}::%x", 0));
                    {% endfor %}
                    return {true};
                }

                bool moreThanOneBit = !HasZeroOrOneBits(value);
                if (moreThanOneBit) {
                    s->Append("(");
                }

                bool first = true;
                {% for value in type.values if value.value != 0 %}
                    if (value & {{as_cppType(type.name)}}::{{as_cppEnum(value.name)}}) {
                        if (!first) {
                            s->Append("|");
                        }
                        first = false;
                        s->Append("{{as_cppEnum(value.name)}}");
                        value &= ~{{as_cppType(type.name)}}::{{as_cppEnum(value.name)}};
                    }
                {% endfor %}

                if (static_cast<bool>(value)) {
                    if (!first) {
                        s->Append("|");
                    }
                    s->Append(absl::StrFormat("{{as_cppType(type.name)}}::%x", static_cast<typename std::underlying_type<{{as_cppType(type.name)}}>::type>(value)));
                }

                if (moreThanOneBit) {
                    s->Append(")");
                }
            } else {
                s->Append(absl::StrFormat("%u", static_cast<typename std::underlying_type<{{as_cppType(type.name)}}>::type>(value)));
            }
            return {true};
        }
    {% endfor %}

}  // namespace {{namespace}}
