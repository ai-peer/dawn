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

//* Builder error callbacks
{% for type in by_category["object"] if type.is_builder%}
    static void Forward{{type.name.CamelCase()}}ToClient(dawnBuilderErrorStatus status, const char* message, dawnCallbackUserdata userdata1, dawnCallbackUserdata userdata2);
{% endfor %}

{% for type in by_category["object"] if type.is_builder%}
    {% set Type = type.name.CamelCase() %}
    void On{{Type}}Error(dawnBuilderErrorStatus status, const char* message, uint32_t id, uint32_t serial);
{% endfor %}

//* Command handlers
{% for command in by_category["command"] if command.name.CamelCase() not in client_side_commands %}
    bool Handle{{command.name.CamelCase()}}(const char** commands, size_t* size);
{% endfor %}

{% for CommandName in server_custom_pre_handler_commands %}
    bool PreHandle{{CommandName}}(const {{CommandName}}Cmd& cmd);
{% endfor %}

//* Command doers
{% for command in by_category["command"] %}
    bool Do{{command.name.CamelCase()}}(
        {%- for input in command.inputs -%}
            {{as_annotated_cType(input)}}
            {%- if len(command.outputs) or not loop.last -%}, {%- endif -%}
        {%- endfor -%}
        {%- for output in command.outputs -%}
            {{as_cType(output.type.name)}}* {{as_varName(output.name)}}
            {%- if not loop.last -%}, {%- endif -%}
        {%- endfor -%}
    );
{% endfor %}
