package {{ metadata.kotlin_package }}

{%- macro to_kotlin_type2(type) -%}
    {%- if type.annotation != 'const*' -%}
        {%- if type.category in ['object', 'structure'] -%}
            {{ type.name.CamelCase() }}
        {%- elif type.category in ['bitmask', 'enum'] -%}
            Int
        {%- else -%}
            {{- to_kotlin_type(type.name.get()) -}}
        {%- endif -%}
    {%- endif -%}
{%- endmacro -%}

{% macro render_method(method) %}
    external fun {{ method.name.camelCase() }}(
    {%- for arg in method.arguments if arg.name.get() != 'userdata' -%}
        {{- ',' if not loop.first -}}
        {{- arg.name.camelCase() }}:
        {%- if arg.annotation == 'const*' -%}
            {%- if arg.type.category == 'structure' -%}
                {{- arg.type.name.CamelCase() -}}?
            {%- elif arg.type.category == 'object' -%}
                Array<{{- arg.type.name.CamelCase() }}>
            {%- else -%}
                {{- to_kotlin_type_multi(arg.type.name.get()) -}}?
            {%- endif -%}
        {% else %}
            {%- if arg.type.category in ['object', 'structure'] -%}
                {{ arg.type.name.CamelCase() }}
            {%- elif arg.type.category == 'function pointer' -%}
                {{ arg.type.name.CamelCase() }}
            {%- elif arg.type.category in ['bitmask', 'enum'] -%}
                Int
            {%- else -%}
                {{- to_kotlin_type(arg.type.name.get()) -}}
            {% endif %}
        {% endif %}
    {%- endfor -%}):
    {{- to_kotlin_type2(method.return_type) -}}

{% endmacro %}

{% if object %}
    class {{ object.name.CamelCase() }}(val handle: Long) {

        {% for method in object.methods %}
            {% if method.name.get().startswith('get ') and not method.arguments %}
                val {{ method.name.camelCase()[len('get')] | lower }}{{method.name.camelCase()[len('get') + 1:] }}: {{ to_kotlin_type2(method.return_type) }}
                external get
            {% else %}
                {{ render_method(method) }}
            {% endif %}
        {% endfor %}
    }
{% else %}
    object Functions {
        init {
            System.loadLibrary("webgpu_c_bundled");
        }
        {% for function in by_category['function'] %}
            {{ render_method(function) }}
        {% endfor %}
    }
{% endif %}
