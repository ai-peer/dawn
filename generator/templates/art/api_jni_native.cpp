#include <jni.h>
#include "dawn/webgpu.h"
#include "Converter.h"

#define DEFAULT __attribute__((visibility("default")))

struct UserData {
    JNIEnv *env;
    jobject callback;
};

{%- macro invoke(object, method) %}
    {%- if object %}
        wgpu{{ object.name.CamelCase() }}{{ method.name.CamelCase() }}(handle
        {{- ', ' if method.arguments }}
    {%- else %}
        wgpu{{ method.name.CamelCase() }}(
    {%- endif %}
    {%- for arg in method.arguments -%}
        {%- if arg.annotation == 'const*' %}
            {%- if arg.type.category == 'object' %}
                reinterpret_cast<{{ as_cType(arg.type.name) }}*>(native{{ arg.name.CamelCase() }})
            {%- elif arg.type.category == 'structure' %}
                native{{ arg.name.CamelCase() }}
            {%- elif arg.type.name.get() in ['char', 'uint8_t', 'uint32_t', 'void'] %}
                native{{ arg.name.CamelCase() }}
            {%- else %}
                // unknown
            {%- endif %}
        {%- else %}
            {%- if arg.type.category == 'object' %}
                native{{ arg.name.CamelCase() }}
            {%- elif arg.type.category in ['bitmask', 'enum'] %}
                ({{ as_cType(arg.type.name) }}) {{ arg.name.camelCase() }}
            {%- elif arg.type.category == 'function pointer' %}
                native{{ arg.name.CamelCase() }}
            {%- elif arg.name.get() == 'userdata' and arg.type.name.get() == 'void *' %}
                nativeUserdata
            {%- else %}
                {{ arg.name.camelCase() }}
            {%- endif %}
        {%- endif %}
        {{ ',' if not loop.last }}
    {%- endfor -%}
    )
{%- endmacro -%}

{%- macro render_method(method, object) %}
    DEFAULT extern "C"{{ ' ' }}
    {%- if method.return_type.annotation != 'const*' -%}
        {%- if method.return_type.category == 'structure' -%}
            /* nope1 */
        {%- elif method.return_type .category in ['bitmask', 'enum'] -%}
            jint
        {%- elif method.return_type.category == 'object' -%}
            jobject
        {%- else -%}
            {{ to_jni_type(method.return_type.name.get()) }}
        {%- endif -%}
    {%- endif %}
    {{ ' ' }}Java_{{ metadata.kotlin_jni_path }}_
            {{- object.name.CamelCase() if object else 'FunctionsKt' -}}
            _{{ method.name.camelCase() }}(
           JNIEnv *env {{ ', jobject obj' if object else ', jclass clazz' -}}
        {%- for arg in method.arguments if arg.name.get() != 'userdata'
                and not method.arguments | selectattr('length', 'equalto', arg) | first -%},
            {%- if arg.annotation == 'const*' %}
                {%- if arg.type.category == 'structure' -%}
                    jobject
                {%- elif arg.type.category == 'object' -%}
                    jobjectArray
                {%- elif arg.type.name.get() == 'void' -%}
                    jobject
                {%- else -%}
                    {{ {'char': 'jstring',
                        'uint8_t': 'jbyteArray',
                        'uint32_t': 'jintArray'}[arg.type.name.get()] }}
                {%- endif %}
            {%- else %}
                {%- if arg.type.category == 'object' -%}
                    jobject
                {%- elif arg.type.category in ['bitmask', 'enum'] -%}
                    jint
                {%- elif arg.type.category in ['function pointer', 'structure'] -%}
                    jobject
                {%- else -%}
                    {{ to_jni_type(arg.type.name.get()) }}
                {%- endif %}
            {%- endif -%}
            {{ ' ' }}{{ arg.name.camelCase() }}
        {%- endfor %}) {

        {% if object %}
            jclass {{ object.name.camelCase() }}Class =
                    env->FindClass("{{ metadata.kotlin_path }}/{{ object.name.CamelCase() }}");
            const {{ as_cType(object.name) }} handle = reinterpret_cast<{{ as_cType(object.name) }}>
                    (env->CallLongMethod(obj, env->GetMethodID({{ object.name.camelCase() }}Class,
                    "getHandle", "()J")));
        {% endif %}
        {% for type in method.arguments|map(attribute='type')|
                selectattr('category', 'equalto', 'object')|unique %}
            jclass {{ type.name.camelCase() }}Class =
                    env->FindClass("{{ metadata.kotlin_path }}/{{ type.name.CamelCase() }}");
            jmethodID {{ type.name.camelCase() }}GetHandle =
                    env->GetMethodID({{ type.name.camelCase() }}Class, "getHandle", "()J");
        {% endfor %}

        {%- for arg in method.arguments %}
            {% if arg.annotation == 'const*' %}
                {% if arg.type.category == 'structure' %}
                    const {{ as_cType(arg.type.name) }}* native{{ arg.name.CamelCase() }} =
                            {{ arg.name.camelCase() }} ? new {{ as_cType(arg.type.name) }}
                            (convert{{ arg.type.name.CamelCase() }}
                            (env, {{ arg.name.camelCase() }})) : nullptr;
                {% elif arg.type.category == 'object' %}
                    size_t {{ arg.name.camelCase() }}Count =
                            env->GetArrayLength({{ arg.name.camelCase() }});
                    {{ as_cType(arg.type.name) }}* native{{ arg.name.CamelCase() }} =
                            new {{ as_cType(arg.type.name) }}[{{ arg.name.camelCase() }}Count];
                    for (int idx = 0; idx != {{ arg.name.camelCase() }}Count; idx++) {
                        native{{ arg.name.CamelCase() }}[idx] =
                                reinterpret_cast<{{ as_cType(arg.type.name) }}>(env->CallLongMethod(
                                env->GetObjectArrayElement({{ arg.name.camelCase() }}, idx),
                                {{ arg.type.name.camelCase() }}GetHandle));
                    }
                {% elif arg.type.name.get() == 'void' %}
                    jbyte *native{{ arg.name.CamelCase() }} =
                            (jbyte *) env->GetDirectBufferAddress(data);
                {% elif arg.type.name.get() == 'char' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} =
                            {{ arg.name.camelCase() }} ? env->GetStringUTFChars(
                            static_cast<jstring>({{ arg.name.camelCase() }}), 0) : nullptr;
                {% elif arg.type.name.get() == 'uint8_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} =
                            {{ arg.name.camelCase() }} ?
                            reinterpret_cast<const {{ arg.type.name.get() }} *>
                            (env->GetByteArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% elif arg.type.name.get() == 'uint32_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} =
                            {{ arg.name.camelCase() }} ?
                            reinterpret_cast<const {{ arg.type.name.get() }} *>
                            (env->GetIntArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% elif arg.type.name.get() == 'uint64_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} =
                            {{ arg.name.camelCase() }} ?
                            reinterpret_cast<const {{ arg.type.name.get() }} *>
                            (env->GetLongArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% endif %}
            {% else %}
                {% if arg.type.category == 'object' %}
                    const {{ as_cType(arg.type.name) }} native{{ arg.name.CamelCase() }} =
                            reinterpret_cast<{{ as_cType(arg.type.name) }}>
                            (env->CallLongMethod({{ arg.name.camelCase() }},
                            {{ arg.type.name.camelCase() }}GetHandle));
                {% elif arg.type.category == 'function pointer' %}
                    struct UserData *nativeUserdata = new UserData(
                            {.env = env, .callback = env->NewGlobalRef({{ arg.name.camelCase() }})});

                    {{ as_cType(arg.type.name) }} native{{ arg.name.CamelCase() }} = [](
                    {%- for arg0 in arg.type.arguments %}
                        {{ as_annotated_cType(arg0) }}{{ ',' if not loop.last }}
                    {%- endfor %}) {
                    UserData* userData1 = static_cast<UserData *>(userdata);
                    JNIEnv *env = userData1->env;
                    if (env->ExceptionCheck()) {
                        return;
                    }
                    jclass {{ arg.type.name.camelCase() }}Class = env->FindClass(
                            "{{ metadata.kotlin_path }}/{{ arg.type.name.CamelCase() }}");
                    {% for arg0 in arg.type.arguments if arg0.type.category == 'object' %}
                        jclass {{ arg0.type.name.camelCase() }}Class = env->FindClass(
                                "{{ metadata.kotlin_path }}/{{ arg0.type.name.CamelCase() }}");
                    {% endfor %}
                    jmethodID id =
                            env->GetMethodID({{ arg.type.name.camelCase() }}Class, "callback", "(
                    {%- for arg0 in arg.type.arguments if arg0.name.get() != 'userdata'-%}
                        {%- if arg0.type.category in ['bitmask', 'enum'] -%}
                            I
                        {%- elif arg0.type.category in ['object', 'structure'] -%}
                            L{{ metadata.kotlin_path }}/{{ arg0.type.name.CamelCase() }};
                        {%- elif arg0.type.name.get() == 'char' -%}
                            Ljava/lang/String;
                        {%- else -%}
                            $
                        {%- endif -%}
                    {%- endfor %})V");
                    env->CallVoidMethod(userData1->callback, id
                    {%- for arg0 in arg.type.arguments if arg0.name.get() != 'userdata'%},
                        {%- if arg0.type.category == 'object' %}
                            env->NewObject({{ arg0.type.name.camelCase() }}Class,
                                    env->GetMethodID({{ arg0.type.name.camelCase() }}Class,
                                    "<init>", "(J)V"), reinterpret_cast<jlong>(
                                    {{ arg0.name.camelCase() }}))
                        {%- elif arg0.type.category in ['bitmask', 'enum'] %}
                            jint({{ arg0.name.camelCase() }})
                        {%- elif arg0.type.category == 'structure' %}
                            {# TODO: convert structures to Kotlin objects #}
                            jlong({{ arg0.name.camelCase() }})
                        {%- elif arg0.type.name.get() == 'char' %}
                            env->NewStringUTF({{ arg0.name.camelCase() }})
                        {%- else %}
                            {{ arg0.name.camelCase() }}
                        {%- endif %}
                    {%- endfor %});
                    };
                {% else %}
                    {%- set length_of =
                            method.arguments | selectattr('length', 'equalto', arg) | first %}
                    {% if length_of and length_of.type.name.get() == 'void' %}
                        const size_t {{ arg.name.camelCase() }} =
                                {{ length_of.name.camelCase() }} ?
                                env->GetDirectBufferCapacity({{ length_of.name.camelCase() }}) : 0;
                    {% elif length_of %}
                       const size_t {{ arg.name.camelCase() }} = {{ length_of.name.camelCase() }} ?
                                env->GetArrayLength({{ length_of.name.camelCase() }}) : 0;
                    {% endif %}
                {% endif %}
            {% endif %}
        {%- endfor %}

        {%- if method.return_type.name.get() == 'void' %}
            {{ invoke(object, method) }};
        {%- else %}
            auto result = {{ invoke(object, method) }};
            if (env->ExceptionCheck()) {
                {% if method.return_type.category in ['function pointer', 'object', 'structure'] %}
                    return nullptr;
                {% else %}
                    return 0;
                {% endif %}
            }
        {%- endif %}

        {%- for arg in method.arguments %}
            {%- if arg.annotation == 'const*' %}
                {%- if arg.type.name.get() == 'char' %}
                    env->ReleaseStringUTFChars({{ arg.name.camelCase() }},
                            native{{ arg.name.CamelCase() }});
                {%- endif %}
            {%- endif %}
        {%- endfor %}

        {% if method.return_type.category == 'object' %}
            jclass {{ method.return_type.name.camelCase() }}Class = env->FindClass(
                    "{{ metadata.kotlin_path }}/{{ method.return_type.name.CamelCase() }}");
            return env->NewObject({{ method.return_type.name.camelCase() }}Class,
                    env->GetMethodID({{ method.return_type.name.camelCase() }}Class,
                    "<init>", "(J)V"), reinterpret_cast<jlong>(result));
        {% elif method.return_type.name.get() != 'void' %}
            return result;
        {% endif %}
    }
{%- endmacro %}

{%- for function in by_category['function'] %}
    {{ render_method(function, None) }}
{%- endfor %}
