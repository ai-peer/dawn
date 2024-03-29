package {{ metadata.kotlin_package }}

fun interface {{ function_pointer.name.CamelCase() }} {
fun callback(
    {%- for arg in function_pointer.arguments if arg.name.get() != 'userdata' %}
        {{- ',' if not loop.first }}
        {{ arg.name.camelCase() }}:
        {%- if arg.annotation == 'const*' %}
            {%- if arg.type.category == 'structure' -%}
                Long {#- TODO: allow structures to be sent to Kotlin #}
            {%- elif arg.type.category == 'object' -%}
                Array<{{ arg.type.name.CamelCase() }}>
            {%- else -%}
                {{ to_kotlin_type_multi(arg.type.name.get()) }}
            {%- endif -%}
        {%- else -%}
            {%- if arg.type.category == 'structure' -%}
                Unknown0:{{ arg.type.name.get() }}
            {%- elif arg.type.category == 'object' -%}
                {{ arg.type.name.CamelCase() }}
            {%- elif arg.type.category in ['bitmask', 'enum'] -%}
                Int
            {%- else -%}
                {{ to_kotlin_type(arg.type.name.get()) }}
            {% endif %}
        {% endif %}
        {%- if arg.optional or arg.default_value == 'nullptr' or arg.type.category in ['function pointer', 'object'] -%}
            ?
        {%- endif -%}
    {%- endfor -%});
}
