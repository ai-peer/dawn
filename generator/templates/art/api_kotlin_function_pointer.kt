package {{ metadata.kotlin_package }}
{% from 'art/api_kotlin_types.kt' import kotlin_declaration with context %}

fun interface {{ function_pointer.name.CamelCase() }} {
@Suppress("INAPPLICABLE_JVM_NAME")  {# Required for @JvnName on global function #}
@JvmName("callback")  {# Required to access Inline Value Class parameters via JNI #}
fun callback(
    {# userdata parameter is omitted because Kotlin callbacks can achieve the same with closures #}
    {%- for arg in function_pointer.arguments if arg.name.get() != 'userdata' %}
        {{- ',' if not loop.first }}{{ arg.name.camelCase() }}:{{ kotlin_declaration(arg, false) }}
    {%- endfor -%});
}
