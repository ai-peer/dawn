package {{ metadata.kotlin_package }}

{% macro get_kotlin_type(member, structure) %}
    {% if structure.name.get() == 'surface descriptor from android native window' and member.name.get() == 'window' %}
        Long
    {% elif member.annotation == 'const*' %}
        {% if member.type.category in ['object', 'structure'] %}
            {% if member.length == 'constant' %}
                {{ member.type.name.CamelCase() }}
            {% else %}
                Array<{{ member.type.name.CamelCase() }}>
            {% endif %}
        {% elif member.type.category in ['bitmask', 'enum'] %}
            IntArray
        {% else %}
            {{ to_kotlin_type_multi(member.type.name.get()) }}
        {% endif %}
    {% elif member.annotation == '*' %}
        {{ {'void': 'Array<Byte>'}.get(member.type.name.get(), 'Unknown16p:' + member.type.name.get()) }}
    {% else %}
        {% if member.type.category in ['function pointer', 'object'] %}
            {{ member.type.name.CamelCase() }}
        {% elif member.type.category == 'structure' %}
            {{ member.type.name.CamelCase() }}
        {% elif member.type.category in ['bitmask', 'enum'] %}
            Int
        {% else %}
            {{ to_kotlin_type(member.type.name.get()) }}
        {% endif %}
    {% endif %}
{% endmacro -%}

{% set class_name = '' + structure.name.CamelCase() %}
{{ 'open ' if chain_children[structure.name.get()] }}class {{ class_name -}}(
    {%- for member in structure.members %}
        {% set type = get_kotlin_type(member, structure) | trim %}
        {% set nullable = member.optional or member.default_value == 'nullptr' or member.type.category in ['function pointer', 'object'] %}
        {% if nullable %}
            {% set type = type + '?' %}
        {% endif %}
        var {{ member.name.camelCase() }}: {{ type }}
        {%- if nullable -%}
            {{' = ' }}null
        {%- elif member.type.category == 'structure' -%}
            {%- if member.annotation == 'const*' and member.length != 'constant' -%}
                {{' = ' }}arrayOf()
            {% else -%}
                {{ ' = ' }}{{ member.type.name.CamelCase() }}()
            {% endif %}
        {% elif member.annotation == '*' %}
            {% if structure.name.get() == 'surface descriptor from android native window' and member.name.get() == 'window' -%}
                {{ ' = 0' }}
            {% elif member.type.name.get() == 'void' -%}
                {{ ' = arrayOf()' }}
            {% endif %}
        {% elif member.annotation == 'const*' %}
            {%- if member.type.name.get() == 'char' -%}
                {{ ' = ' }}{{ 'null' if optional else member.default_value or '""' }}
            {%- elif member.type.category in ['bitmask', 'enum'] %}
                {{' = ' }}intArrayOf()
            {%- else %}
                {{' = ' + {'double': 'doubleArrayOf()',
                           'float': 'floatArrayOf()',
                           'uint8_t': 'byteArrayOf()',
                           'uint32_t': 'intArrayOf()',
                           'uint64_t': 'longArrayOf()'}.get(member.type.name.get(), 'arrayOf()') }}
            {%- endif %}
        {%- elif member.type.category in ['bitmask', 'enum'] %}
            {% if member.default_value %}
                {% for value in member.type.values if value.name.name == member.default_value %}
                    {{ ' = ' }}{{ member.type.name.CamelCase() }}.{{ as_ktName(value.name.CamelCase()) }}
                {% endfor %}
            {%- elif member.type.values[0].value == 0 -%}
                {{ ' = ' }}{{ member.type.name.CamelCase() }}.{{ as_ktName(member.type.values[0].name.CamelCase()) }}
            {%- else -%}
                {{ ' = 0' }}
            {%- endif %}
        {%- elif member.type.category in ['function pointer', 'object'] -%}
            {{' = null' }}
        {%- elif member.type.name.get() in ['void *', 'void const *'] -%}
            {{' = byteArrayOf()' }}
        {%- elif member.type.name.get() == 'bool' -%}
            {{' = ' }}{{ member.default_value or 'false' }}
        {%- elif member.type.name.get() == 'double' -%}
            {{' = ' }}{{ 'Double.NaN' if member.default_value == 'NAN' else member.default_value or '0.0' }}
        {%- elif member.type.name.get() == 'float' -%}
            {{' = ' }}{{ 'Float.NaN' if member.default_value == 'NAN' else member.default_value or '0.0' }}
        {%- elif member.type.name.get() in ['int', 'int16_t', 'int32_t', 'int64_t', 'size_t', 'uint16_t', 'uint32_t', 'uint64_t'] -%}
            {%- if (member.default_value ~ '').startswith('WGPU_') -%}
                {{' = ' }}{{ 'Constants.' + member.default_value | replace('WGPU_', '') }}
            {%- elif member.default_value == '0xFFFFFFFF' -%}
                {{ '= -0x7FFFFFFF' }}
            {%- else -%}
                {{ ' = ' }}{{ member.default_value or '0' }}
            {%- endif %}
        {%- else %}
            /* member.annotation {{ member.annotation }} */
            /* member.type {{ member.type.name.get() }} */
        {% endif -%}
        {{- ',' if not loop.last }}
    {% endfor %}
){% for root in structure.chain_roots -%}
    {{ ':' if loop.first }}{{ root.name.CamelCase() }}()
{%- endfor %} {
}
