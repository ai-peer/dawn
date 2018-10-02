#include "jstrace/jstrace.h"

#include <string>
#include <unordered_map>
#include <iostream>

namespace jstrace {

    {% for type in by_category["object"] %}
        static std::unordered_map<{{as_cType(type.name)}}, std::string> {{type.name.camelCase()}}Names;
        static int {{type.name.camelCase()}}Counts;
    {% endfor %}

    static dawnProcTable procs;

    static Output out;

    void Init(dawnDevice device) {
        Teardown();

        deviceNames[device] = "device";
    }

    void Teardown() {
        {% for type in by_category["object"] %}
            {{type.name.camelCase()}}Names.clear();
            {{type.name.camelCase()}}Counts = 0;
        {% endfor %}

        out.GetOutputAndClear();
    }

    Output* GetOutput() {
        return &out;
    }

    {% for type in by_category["enum"] %}
        void PrettyPrint({{as_cType(type.name)}} value) {
            switch(value) {
                {% for value in type.values %}
                    case {{as_cEnum(type.name, value.name)}}:
                        out << "\"{{value.name.camelCase()}}\"";
                        break;
                {% endfor %}
                default:
                    out << "\"garbage value\"";
                    break;
            }
        }
    {% endfor %}

    {% for type in by_category["bitmask"] %}
        void PrettyPrint({{as_cType(type.name)}} value) {
            if (value == 0) {
                out << "0";
                return;
            }

            bool first = true;
            {% for value in type.values %}
                if (value & {{as_cEnum(type.name, value.name)}}) {
                    if (!first) {
                        out << " | ";
                    }
                    out << "webgpu.{{type.name.SNAKE_CASE()}}_{{value.name.SNAKE_CASE()}}";
                    first = false;
                }
            {% endfor %}
        }
    {% endfor %}

    {% for type in types.itervalues() if type.category != "natively defined"%}
        void OutputOne({{as_cType(type.name)}} const * value);
    {% endfor %}

    template<typename T>
    void OutputList(T const * values, size_t count) {
        out << "[";
        for (size_t i = 0; i < count; ++i) {
            if (i != 0) {
                out << ", ";
            }
            OutputOne(values + i);
        }
        out << "]";
    }

    {% for type in types.itervalues() if type.category != "natively defined"%}
        void OutputOne({{as_cType(type.name)}} const * value) {
            {% if type.category == "native" and type.name.canonical_case() != "void" %}
                out << std::to_string(*value);
            {% elif type.category == "object" %}
                out << {{type.name.camelCase()}}Names[*value];
            {% elif type.category in ["enum", "bitmask"] %}
                PrettyPrint(*value);
            {% elif type.category == "structure" %}
                out << "{ ";
                {% for member in type.members %}
                    {% if not loop.first %}
                        out << ", ";
                    {% endif %}
                    out << "\"{{as_varName(member.name)}}\": ";
                    {% if member.annotation == "value" %}
                        OutputOne(&value->{{as_varName(member.name)}});
                    {% elif member.annotation == "const*" %}
                        {% if member.length == "constant" %}
                            {% if member.constant_length == 1 %}
                                OutputOne(value->{{as_varName(member.name)}});
                            {% else %}
                                OutputList(value->{{as_varName(member.name)}}, {{member.constant_length}});
                            {% endif %}
                        {% elif member.length == "strlen" %}
                            out << "\"" << value->{{as_varName(member.name)}} << "\"";
                        {% else %}
                            OutputList(value->{{as_varName(member.name)}}, value->{{as_varName(member.length.name)}});
                        {% endif %}
                    {% else %}
                        out << "/*Unhandled annotation*/";
                    {% endif %}
                {% endfor %}
                out << " }";
            {% else %}
                out << "Unhandled Output";
            {% endif %}
        }
    {% endfor %}

    {% for type in by_category["object"] %}
        {% for method in type.methods %}
            {% set Suffix = as_MethodSuffix(type.name, method.name) %}

            {{as_cType(method.return_type.name)}} Trace{{Suffix}}(
                {{-as_cType(type.name)}} self
                {%- for arg in method.arguments -%}
                    , {{as_annotated_cType(arg)}}
                {%- endfor -%}
            ) {
                {% if method.return_type.category == "object" %}
                    std::string resultName = "{{method.return_type.name.camelCase()}}" + std::to_string({{method.return_type.name.camelCase()}}Counts++);
                    out << resultName << " = ";
                    {{as_cType(method.return_type.name)}} result =
                {% endif %}
                procs.{{as_varName(type.name, method.name)}}(self
                    {%- for arg in method.arguments -%}
                        , {{as_varName(arg.name)}}
                    {%- endfor -%}
                );

                out << {{type.name.camelCase()}}Names[self] << "." << "{{method.name.camelCase()}}(";
                {% for arg in method.arguments %}
                    {% if not loop.first %}
                        out << ", ";
                    {% endif %}
                    {% if arg.annotation == "value" %}
                        OutputOne(&{{as_varName(arg.name)}});
                    {% elif arg.annotation == "const*" %}
                        {% if arg.length == "constant" %}
                            {% if arg.constant_length == 1 %}
                                OutputOne({{as_varName(arg.name)}});
                            {% else %}
                                OutputList({{as_varName(arg.name)}}, {{arg.constant_length}});
                            {% endif %}
                        {% elif arg.length == "strlen" %}
                            out << "\"" << {{as_varName(arg.name)}} << "\"";
                        {% else %}
                            OutputList({{as_varName(arg.name)}}, {{as_varName(arg.length.name)}});
                        {% endif %}
                    {% else %}
                        out << "/*Unhandled annotation*/";
                    {% endif %}
                {% endfor %}
                out << ");" << "\n";

                {% if method.return_type.category == "object" %}
                    {{method.return_type.name.camelCase()}}Names[result] = resultName;
                    return result;
                {% endif %}
            }
        {% endfor %}
    {% endfor %}

    dawnProcTable GetProcs(const dawnProcTable* originalProcs) {
        procs = *originalProcs;

        dawnProcTable table = *originalProcs;
        {% for type in by_category["object"] %}
            {% for method in type.methods %}
                {% set suffix = as_MethodSuffix(type.name, method.name) %}
                table.{{as_varName(type.name, method.name)}} = reinterpret_cast<{{as_cProc(type.name, method.name)}}>(Trace{{suffix}});
            {% endfor %}
        {% endfor %}
        return table;
    }

    const char* GetBufferName(dawnBuffer buffer) {
        return bufferNames[buffer].c_str();
    }

    const char* GetTextureName(dawnTexture texture) {
        return textureNames[texture].c_str();
    }

}  // namespace jstrace
