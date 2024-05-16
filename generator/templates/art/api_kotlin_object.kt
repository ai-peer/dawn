package {{ metadata.kotlin_package }}
{% from 'art/api_kotlin_types.kt' import kotlin_declaration with context %}

{% macro render_method(method) %}
    @JvmName("{{ method.name.camelCase() }}") external fun {{ method.name.camelCase() }}(
    {# userdata parameter is omitted because Kotlin callbacks can achieve the same with closures #}
    {# length parameters are omitted because Kotlin containers have 'length' #}
    {%- for arg in method.arguments if arg.name.get() != 'userdata' and
            not method.arguments | selectattr('length', 'equalto', arg) | first -%}
        {{- arg.name.camelCase() }}:{{ kotlin_declaration(arg, true) }}{{- ',' if not loop.last }}
    {%- endfor -%}):
    {{- kotlin_declaration({"type": method.return_type}) -}}
{% endmacro %}

{% if object %}
    class {{ object.name.CamelCase() }}(val handle: Long) {
        {% for method in object.methods %}{{ render_method(method) }}{% endfor %}
    }

    {% for method in object.methods %}
        {% if method.name.get().startswith('get ') and not method.arguments %}
            val {{ object.name.CamelCase() }}.{{ method.name.camelCase()[len('get')] | lower }}
                {{- method.name.camelCase()[len('get') + 1:] }}
                get() = this.{{ method.name.camelCase() }}()
        {% endif %}
    {% endfor %}

{% else %}
    {# Global fuctions #}
    {% for function in by_category['function'] %}{{ render_method(function) }}{% endfor %}
{% endif %}
