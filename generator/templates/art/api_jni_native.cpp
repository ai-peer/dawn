#include <jni.h>
#include "gen/include/dawn/webgpu.h"

#define DEFAULT __attribute__((visibility("default")))

const char *getString(JNIEnv *env, const jobject pJobject) {
    if (!pJobject) {
        return nullptr;
    }
    return env->GetStringUTFChars(static_cast<jstring>(pJobject), 0);
}

struct UserData {
    JNIEnv *env;
    jobject callback;
};

{% for structure in by_category['structure'] %}
    const {{ as_cType(structure.name) }} convert{{ structure.name.CamelCase() }}(JNIEnv *env, jobject obj);
{% endfor %}

{% for structure in by_category['structure'] %}
    const {{ as_cType(structure.name) }} convert{{ structure.name.CamelCase() }}(JNIEnv *env, jobject obj) {
        jclass {{ structure.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ structure.name.CamelCase() }}");
        {% for type in structure.members|map(attribute='type')|selectattr('category', 'equalto', 'object')|unique %}
            jclass {{ type.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ type.name.CamelCase() }}");
            jmethodID {{ type.name.camelCase() }}GetHandle = env->GetMethodID({{ type.name.camelCase() }}Class, "getHandle", "()J");
        {% endfor %}
        {% for member in structure.members %}
            {% if member.annotation == 'const*' %}
                {% if member.type.category == 'object' %}
                    jobjectArray {{ member.name.camelCase() }} = static_cast<jobjectArray>(env->CallObjectMethod(
                            obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}",
                                                  "()[Landroid/dawn/{{ member.type.name.CamelCase() }};")));
                    size_t {{ member.length.name.camelCase() }} = env->GetArrayLength({{ member.name.camelCase() }});
                    {{ as_cType(member.type.name) }}* native{{ member.name.CamelCase() }} = new {{ as_cType(member.type.name) }}[{{ member.length.name.camelCase() }}];
                    for (int idx = 0; idx != {{ member.length.name.camelCase() }}; idx++) {
                        native{{ member.name.CamelCase() }}[idx] = reinterpret_cast<{{ as_cType(member.type.name) }}>(
                                env->CallLongMethod(env->GetObjectArrayElement({{ member.name.camelCase() }}, idx), {{ member.type.name.camelCase() }}GetHandle));
                    }
                {% elif member.type.name.get() == 'float' %}
                    jobject native{{ member.name.CamelCase() }} = env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()[F"));
                {% elif member.type.name.get() == 'uint32_t' %}
                    jobject native{{ member.name.CamelCase() }} = env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()[I"));
                {% elif member.type.name.get() == 'uint64_t' %}
                    jobject native{{ member.name.CamelCase() }} = env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()[J"));
                {% elif member.type.category == 'structure' and member.length != 'constant' %}
                    jobject native{{ member.name.CamelCase() }} = env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()[L{{ metadata.kotlin_path }}/{{ member.type.name.CamelCase() }};"));
                {% elif member.type.category in ['bitmask', 'enum'] %}
                    jobject native{{ member.name.CamelCase() }} = env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}","()[I"));
                {% endif %}
            {% else %}
                {% if member.type.category == 'object' %}
                    jobject {{ member.name.camelCase() }} = static_cast<jobjectArray>(env->CallObjectMethod(
                            obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}",
                                                  "()Landroid/dawn/{{ member.type.name.CamelCase() }};")));
                    {{ as_cType(member.type.name) }} native{{ member.name.CamelCase() }} = {{ member.name.camelCase() }} ?
                        reinterpret_cast<{{ as_cType(member.type.name) }}>(env->CallLongMethod({{ member.name.camelCase() }}, {{ member.type.name.camelCase() }}GetHandle)) : nullptr;
                {% endif %}
            {% endif %}
        {% endfor %}

        {{ as_cType(structure.name) }} converted = {
            {% if len(structure.chain_roots) == 1 %}
                .chain = {.sType = WGPUSType_{{ structure.name.CamelCase() }}},
            {% endif %}
            {% for member in structure.members %}
                {% if member.length.name %}
                    .{{ member.length.name.camelCase() }} =
                    {% if member.type.category == 'object' %}
                        {{ member.length.name.camelCase() }}
                    {% else %}
                        static_cast<{{ as_cType(member.length.type.name) }}>(native{{ member.name.CamelCase() }} ? env->GetArrayLength(static_cast<jarray>(native{{ member.name.CamelCase() }})) : 0)
                    {% endif %},
                {% endif %}
                .{{ member.name.camelCase() }} ={{ ' ' -}}
                {% if member.annotation == 'const*' %}
                    {%- if member.type.name.get() == 'char' -%}
                        getString(env, env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()Ljava/lang/String;")))
                    {%- elif member.type.category == 'object' -%}
                        native{{ member.name.CamelCase() }}
                    {%- elif member.type.name.get() == 'float' -%}
                        env->GetFloatArrayElements(static_cast<jfloatArray>(native{{ member.name.CamelCase() }}), 0)
                    {%- elif member.type.name.get() == 'uint32' -%}
                        env->GetIntArrayElements(static_cast<jintArray>(native{{ member.name.CamelCase() }}), 0)
                    {%- elif member.type.category == 'structure' -%}
                        {%- if member.length == 'constant' -%}
                            convert{{ member.type.name.CamelCase() }}Optional(env, env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()L{{ metadata.kotlin_path }}/{{ member.type.name.CamelCase() }};")))
                        {%- else -%}
                            convert{{ member.type.name.CamelCase() }}Array(env, static_cast<jobjectArray>(native{{ member.name.CamelCase() }}))
                        {%- endif -%}
                    {%- elif member.type.category in ['bitmask', 'enum'] -%}
                        reinterpret_cast<{{ as_cType(member.type.name) }}*>(native{{ member.name.CamelCase() }})
                    {%- else -%}
                        nullptr /* unknown. annotation: {{ member.annotation }} category: {{ member.type.category }} name: {{ member.type.name.get() }} */
                    {% endif -%}
                {%- elif member.type.name.get() == 'bool' -%}
                    env->CallBooleanMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()Z"))
                {%- elif member.type.name.get() == 'double' -%}
                    env->CallDoubleMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()D"))
                {%- elif member.type.name.get() == 'float' -%}
                    env->CallFloatMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()F"))
                {%- elif member.type.name.get() == 'uint16_t' -%}
                    static_cast<uint16_t>(env->CallShortMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()S")))
                {%- elif member.type.name.get() == 'int' -%}
                    env->CallIntMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()I"))
                {%- elif member.type.name.get() == 'int32_t' -%}
                    static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.name.get() == 'uint32_t' -%}
                    static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.name.get() == 'uint64_t' -%}
                    static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()J")))
                {%- elif member.type.name.get() == 'size_t' -%}
                    static_cast<size_t>(env->CallLongMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()J")))
                {%- elif member.type.category in ['bitmask', 'enum'] -%}
                    static_cast<{{ as_cType(member.type.name) }}>(env->CallIntMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.category == 'object' -%}
                    native{{ member.name.CamelCase() }}
                {%- elif member.type.category == 'structure' -%}
                    convert{{ member.type.name.CamelCase() }}(env, env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()L{{ metadata.kotlin_path }}/{{ member.type.name.CamelCase() }};")))
                {%- elif structure.name.get() == 'surface descriptor from android native window' and member.name.get() == 'window' -%}
                    reinterpret_cast<void*>(env->CallLongMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class, "get{{ member.name.CamelCase() }}", "()J")))
                {%- else -%}
                    nullptr /* unknown. annotation: {{ member.annotation }} category: {{ member.type.category }} name: {{ member.type.name.get() }} */
                {%- endif %}
            {{- ',' if not loop.last }}
            {% endfor %}
        };

        {% for child in chain_children[structure.name.get()] %}
            jclass {{ child.name.CamelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ child.name.CamelCase() }}");
            if (env->IsInstanceOf(obj, {{ child.name.CamelCase() }}Class)) {
                 converted.nextInChain = &(new {{ as_cType(child.name) }}(convert{{ child.name.CamelCase() }}(env, obj)))->chain;
            }
        {% endfor %}
        return converted;
    }

    const {{ as_cType(structure.name) }}* convert{{ structure.name.CamelCase() }}Optional(JNIEnv *env, jobject obj) {
        if (!obj) {
            return nullptr;
        }
        return new {{ as_cType(structure.name) }}(convert{{ structure.name.CamelCase() }}(env, obj));
    }

    const {{ as_cType(structure.name) }}* convert{{ structure.name.CamelCase() }}Array(JNIEnv *env, jobjectArray arr) {
        if (!arr) {
            return nullptr;
        }
        jsize len = env->GetArrayLength(arr);
        {{ as_cType(structure.name) }}* entries = new {{ as_cType(structure.name) }}[len];
        for (int idx = 0; idx != len; idx++) {
            entries[idx] = convert{{ structure.name.CamelCase() }}(env, env->GetObjectArrayElement(arr, idx));
        }
        return entries;
    }

{% endfor %}

{%- macro invoke(object, method) %}
    {% if object %}
        wgpu{{ object.name.CamelCase() }}{{ method.name.CamelCase() }}(handle
        {{ ', ' if method.arguments }}
    {%- else %}
        wgpu{{ method.name.CamelCase() }}(
    {%- endif %}
    {%- for arg in method.arguments -%}
        {% if arg.annotation == 'const*' %}
            {% if arg.type.category == 'object' %}
                reinterpret_cast<{{ as_cType(arg.type.name) }}*>(native{{ arg.name.CamelCase() }})
            {% elif arg.type.category == 'structure' %}
                native{{ arg.name.CamelCase() }}
            {% elif arg.type.name.get() in ['char', 'uint8_t', 'uint32_t', 'void'] %}
                native{{ arg.name.CamelCase() }}
            {% else %}
                // unknown
            {% endif %}
        {% else %}
            {% if arg.type.category == 'object' %}
                native{{ arg.name.CamelCase() }}
            {% elif arg.type.category in ['bitmask', 'enum'] %}
                ({{ as_cType(arg.type.name) }}) {{ arg.name.camelCase() }}
            {% elif arg.type.category == 'function pointer' %}
                native{{ arg.name.CamelCase() }}
            {% elif arg.name.get() == 'userdata' and arg.type.name.get() == 'void *' %}
                &nativeUserdata
            {% else %}
                {{ arg.name.camelCase() }}
            {% endif %}
        {% endif %}
        {{ ',' if not loop.last }}
    {%- endfor -%}
    )
{%- endmacro -%}

{% macro render_method(method, object) %}
    DEFAULT extern "C"{{' '}}
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
    {{' '}}Java_{{ metadata.kotlin_jni_path }}_{{ object.name.CamelCase() if object else 'Functions' }}_{{ method.name.camelCase() }}(
           JNIEnv *env {{ ', jobject obj' if object else ', jclass clazz' }}
        {%- for arg in method.arguments if arg.name.get() != 'userdata' -%},
            {% if arg.annotation in ['const*'] %}
                {% if arg.type.category == 'structure' %}
                    jobject
                {% elif arg.type.category == 'object' %}
                    jobjectArray
                {% else %}
                    {{ {'char': 'jstring',
                        'uint8_t': 'jbyteArray',
                        'uint32_t': 'jintArray',
                        'void': 'jbyteArray'}.get(arg.type.name.get() , 'Unknown5:' + arg.type.name.get()) }}
                {% endif %}
            {% else %}
                {% if arg.type.category == 'object' %}
                    jobject
                {% elif arg.type.category in ['bitmask', 'enum'] %}
                    jint
                {% elif arg.type.category in ['function pointer', 'structure'] %}
                    jobject
                {% else %}
                    {{ to_jni_type(arg.type.name.get()) }}
                {% endif %}
            {% endif %}
            {{' '}}{{ arg.name.camelCase() }}
        {%- endfor %}) {

        {% if object %}
            jclass {{ object.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ object.name.CamelCase() }}");
            const {{ as_cType(object.name) }} handle = reinterpret_cast<{{ as_cType(object.name) }}>(env->CallLongMethod(obj, env->GetMethodID({{ object.name.camelCase() }}Class, "getHandle", "()J")));
        {% endif %}
        {% for type in method.arguments|map(attribute='type')|selectattr('category', 'equalto', 'object')|unique %}
            jclass {{ type.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ type.name.CamelCase() }}");
            jmethodID {{ type.name.camelCase() }}GetHandle = env->GetMethodID({{ type.name.camelCase() }}Class, "getHandle", "()J");
        {% endfor %}

        {% for arg in method.arguments %}
            {% if arg.annotation == 'const*' %}
                {% if arg.type.category == 'structure' %}
                    const {{ as_cType(arg.type.name) }}* native{{ arg.name.CamelCase() }} = {{ arg.name.camelCase() }} ? new {{ as_cType(arg.type.name) }}(convert{{ arg.type.name.CamelCase() }}(env, {{ arg.name.camelCase() }})) : nullptr;
                {% elif arg.type.category == 'object' %}
                    size_t {{ arg.name.camelCase() }}Count = env->GetArrayLength({{ arg.name.camelCase() }});
                    {{ as_cType(arg.type.name) }}* native{{ arg.name.CamelCase() }} = new {{ as_cType(arg.type.name) }}[{{ arg.name.camelCase() }}Count];
                    for (int idx = 0; idx != {{ arg.name.camelCase() }}Count; idx++) {
                        native{{ arg.name.CamelCase() }}[idx] = reinterpret_cast<{{ as_cType(arg.type.name) }}>(env->CallLongMethod(
                                env->GetObjectArrayElement({{ arg.name.camelCase() }}, idx), {{ arg.type.name.camelCase() }}GetHandle));
                    }
                {% elif arg.type.name.get() == 'void' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} = env->GetByteArrayElements({{ arg.name.camelCase() }}, 0);
                {% elif arg.type.name.get() == 'char' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} = getString(env, {{ arg.name.camelCase() }});
                {% elif arg.type.name.get() == 'uint8_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} = {{ arg.name.camelCase() }} ? reinterpret_cast<const {{ arg.type.name.get() }} *>(env->GetByteArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% elif arg.type.name.get() == 'uint32_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} = {{ arg.name.camelCase() }} ? reinterpret_cast<const {{ arg.type.name.get() }} *>(env->GetIntArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% elif arg.type.name.get() == 'uint64_t' %}
                    const {{ arg.type.name.get() }} *native{{ arg.name.CamelCase() }} = {{ arg.name.camelCase() }} ? reinterpret_cast<const {{ arg.type.name.get() }} *>(env->GetLongArrayElements({{ arg.name.camelCase() }}, 0)) : nullptr;
                {% endif %}
            {% else %}
                {%- if arg.type.category == 'object' %}
                    const {{ as_cType(arg.type.name) }} native{{ arg.name.CamelCase() }} = reinterpret_cast<{{ as_cType(arg.type.name) }}>(env->CallLongMethod({{ arg.name.camelCase() }}, env->GetMethodID({{ arg.type.name.camelCase() }}Class, "getHandle", "()J")));
                {%- elif arg.type.category == 'function pointer' %}
                    struct UserData nativeUserdata = {.env = env, .callback = callback};

                    {{ as_cType(arg.type.name) }} native{{ arg.name.CamelCase() }} = [](
                    {% for arg0 in arg.type.arguments %}
                        {{ as_annotated_cType(arg0) }}{{ ',' if not loop.last }}
                    {% endfor %}) {
                    UserData* userData1 = static_cast<UserData *>(userdata);
                    JNIEnv *env = userData1->env;
                    jclass {{ arg.type.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ arg.type.name.CamelCase() }}");
                    {% for arg0 in arg.type.arguments if arg0.type.category == 'object' %}
                        jclass {{ arg0.type.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ arg0.type.name.CamelCase() }}");
                    {% endfor %}
                    jmethodID id = env->GetMethodID({{ arg.type.name.camelCase() }}Class, "callback", "(
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
                    {% endfor %})V");
                    env->CallVoidMethod(userData1->callback, id
                    {% for arg0 in arg.type.arguments if arg0.name.get() != 'userdata'%},
                        {% if arg0.type.category == 'object' %}
                            env->NewObject({{ arg0.type.name.camelCase() }}Class, env->GetMethodID({{ arg0.type.name.camelCase() }}Class, "<init>", "(J)V"), reinterpret_cast<jlong>({{ arg0.name.camelCase() }}))
                        {% elif arg0.type.category in ['bitmask', 'enum'] %}
                            jint({{ arg0.name.camelCase() }})
                        {% elif arg0.type.category == 'structure' %}
                            jlong({{ arg0.name.camelCase() }})  {# TODO: convert structures to Kotlin objects #}
                        {% elif arg0.type.name.get() == 'char' %}
                            env->NewStringUTF({{ arg0.name.camelCase() }})
                        {% else %}
                            {{ arg0.name.camelCase() }}
                        {% endif %}
                    {% endfor %});
                    };
                {% endif %}
            {% endif %}
        {% endfor %}

        {% if method.return_type.name.get() == 'void' %}
            {{ invoke(object, method) }};
        {% else %}
            auto result = {{ invoke(object, method) }};
        {% endif %}

        {% for arg in method.arguments %}
            {% if arg.annotation == 'const*' %}
                {% if arg.type.name.get() == 'char' %}
                    env->ReleaseStringUTFChars({{ arg.name.camelCase() }}, native{{ arg.name.CamelCase() }});
                {% elif arg.type.name.get() == 'void' %}
                    env->ReleaseByteArrayElements({{ arg.name.camelCase() }}, (jbyte*) native{{ arg.name.CamelCase() }}, 0);
                {% endif %}
            {% endif %}
        {% endfor %}

        {%- if method.return_type.category == 'object' %}
            jclass {{ method.return_type.name.camelCase() }}Class = env->FindClass("{{ metadata.kotlin_path }}/{{ method.return_type.name.CamelCase() }}");
            return env->NewObject({{ method.return_type.name.camelCase() }}Class, env->GetMethodID({{ method.return_type.name.camelCase() }}Class, "<init>", "(J)V"), reinterpret_cast<jlong>(result));
        {%- elif method.return_type.name.get() != 'void' %}
            return result;
        {% endif %}
    }
{% endmacro %}

{% for object in by_category['object'] %}
    {% for method in object.methods %}
        {{ render_method(method, object) }}
    {% endfor %}
{% endfor %}

{% for function in by_category['function'] %}
    {{ render_method(function, None) }}
{% endfor %}
