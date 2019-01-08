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

//* Dawn API
{% for type in by_category["object"] %}
    {% set cType = as_cType(type.name) %}
    {% for method in native_methods(type) %}
        friend {{as_cType(method.return_type.name)}} Client{{as_MethodSuffix(type.name, method.name)}}(
          {{-cType}} cSelf
          {%- for arg in method.arguments -%}
              , {{as_annotated_cType(arg)}}
          {%- endfor -%}
        );
    {% endfor %}
{% endfor %}

//* Dawn API implementation
{% for type in by_category["object"] %}
    {% set Type = type.name.CamelCase() %}
    {% set cType = as_cType(type.name) %}

    {% for method in type.methods + type.native_methods %}
        {{as_cType(method.return_type.name)}} {{as_MethodSuffix(type.name, method.name)}}(
            {{-cType}} cSelf
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}

    {% if type.is_builder %}
        void {{as_MethodSuffix(type.name, Name("set error callback"))}}(
          {{cType}} cSelf,
          dawnBuilderErrorCallback callback,
          dawnCallbackUserdata userdata1,
          dawnCallbackUserdata userdata2
        );
    {% endif %}

    void {{as_MethodSuffix(type.name, Name("release"))}}({{cType}} cObj);
    void {{as_MethodSuffix(type.name, Name("reference"))}}({{cType}} cObj);
{% endfor %}

//* Return command handlers
{% for command in by_category["return command"] %}
    bool Handle{{command.name.CamelCase()}}(const char** commands, size_t* size);
{% endfor %}

//* Return command doers
{% for command in by_category["return command"] %}
    bool Do{{command.name.CamelCase()}}(
        {%- for input in command.inputs -%}
            {%- if input.type.category == "object" and input.annotation == "handle" -%}
                {{as_cppType(input.type.name)}}* {{as_varName(input.name)}}
            {%- else -%}
                {{as_annotated_cppType(input)}}
            {%- endif -%}
            {%- if len(command.outputs) or not loop.last -%}, {% endif %}
        {%- endfor -%}
    );
{% endfor %}
