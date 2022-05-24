//* Copyright 2022 The Dawn Authors
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

{% set PREFIX = metadata.proc_table_prefix.upper() %}

#if !defined({{PREFIX}}_PROC_DEFINITION_IMPL)
static_assert(false, "{{PREFIX}}_PROC_DEFINITION_IMPL must be defined.");
#endif

{% for function in by_category["function"] %}
    {{as_cType(function.return_type.name)}} {{as_cMethod(None, function.name)}}(
        {%- for arg in function.arguments -%}
            {% if not loop.first %}, {% endif %}{{as_annotated_cType(arg)}}
        {%- endfor -%}
    ) {
        return {{PREFIX}}_PROC_DEFINITION_IMPL(
          {{as_varName(function.name)}}
          {%- for arg in function.arguments -%}
              , {{as_varName(arg.name)}}
          {%- endfor -%}
        );
    }
{% endfor %}

{% for type in by_category["object"] %}
    {% for method in c_methods(type) %}
        {{as_cType(method.return_type.name)}} {{as_cMethod(type.name, method.name)}}(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        ) {
            return {{PREFIX}}_PROC_DEFINITION_IMPL(
              {{as_varName(type.name, method.name)}}
              , {{as_varName(type.name)}}
              {%- for arg in method.arguments -%}
                  , {{as_varName(arg.name)}}
              {%- endfor -%}
            );
        }
    {% endfor %}

{% endfor %}
