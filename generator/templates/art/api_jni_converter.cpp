#include <jni.h>
#include "dawn/webgpu.h"
#include "Converter.h"

const char *getString(JNIEnv *env, const jobject pJobject) {
    if (!pJobject) {
        return nullptr;
    }
    return pJobject ? env->GetStringUTFChars(static_cast<jstring>(pJobject), 0) : nullptr;
}

{% macro getNative(structure, member, code) %}
    jobject native{{ member.name.CamelCase() }} =
            env->CallObjectMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class,
            "get{{ member.name.CamelCase() }}", "(){{ code }}"));
{% endmacro %}

{% for structure in by_category['structure'] %}
    const {{ as_cType(structure.name) }} convert{{ structure.name.CamelCase() }}(
            JNIEnv *env, jobject obj) {
        jclass {{ structure.name.camelCase() }}Class = env->FindClass(
                "{{ metadata.kotlin_path }}/{{ structure.name.CamelCase() }}");
        {% for type in structure.members|map(attribute='type')|
                selectattr('category', 'equalto', 'object')|unique %}
            jclass {{ type.name.camelCase() }}Class =
                    env->FindClass("{{ metadata.kotlin_path }}/{{ type.name.CamelCase() }}");
            jmethodID {{ type.name.camelCase() }}GetHandle =
                    env->GetMethodID({{ type.name.camelCase() }}Class, "getHandle", "()J");
        {% endfor %}
        {%- for member in structure.members %}
            {% if member.annotation == 'const*' %}
                {% if member.type.category == 'object' %}
                    jobjectArray {{ member.name.camelCase() }} = static_cast<jobjectArray>(
                            env->CallObjectMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}",
                            "()[Landroid/dawn/{{ member.type.name.CamelCase() }};")));
                    size_t {{ member.length.name.camelCase() }} =
                            env->GetArrayLength({{ member.name.camelCase() }});
                    {{ as_cType(member.type.name) }}* native{{ member.name.CamelCase() }} =
                            new {{ as_cType(member.type.name) }}[
                            {{ member.length.name.camelCase() }}];
                    for (int idx = 0; idx != {{ member.length.name.camelCase() }}; idx++) {
                        native{{ member.name.CamelCase() }}[idx] =
                                reinterpret_cast<{{ as_cType(member.type.name) }}>(
                                env->CallLongMethod(env->GetObjectArrayElement(
                                {{ member.name.camelCase() }}, idx),
                                {{ member.type.name.camelCase() }}GetHandle));
                    }
                {%- elif member.type.name.get() == 'float' -%}
                    {{ getNative(structure, member, '[F') }}
                {%- elif member.type.name.get() == 'uint32_t' -%}
                    {{ getNative(structure, member, '[I') }}
                {%- elif member.type.name.get() == 'uint64_t' -%}
                    {{ getNative(structure, member, '[J') }}
                {%- elif member.type.category == 'structure' and member.length != 'constant' -%}
                    {{ getNative(structure, member, '[L' + metadata.kotlin_path + '/' +
                            member.type.name.CamelCase() + ';') }}
                {%- elif member.type.category in ['bitmask', 'enum'] -%}
                    {{ getNative(structure, member, '[I') }}
                {% endif %}
            {% else %}
                {% if member.type.category == 'object' %}
                    jobject {{ member.name.camelCase() }} = static_cast<jobjectArray>(
                            env->CallObjectMethod(obj, env->GetMethodID(
                                    {{ structure.name.camelCase() }}Class,
                                    "get{{ member.name.CamelCase() }}",
                                    "()Landroid/dawn/{{ member.type.name.CamelCase() }};")));
                    {{ as_cType(member.type.name) }} native{{ member.name.CamelCase() }} =
                            {{ member.name.camelCase() }} ?
                        reinterpret_cast<{{ as_cType(member.type.name) }}>(
                                env->CallLongMethod({{ member.name.camelCase() }},
                                {{ member.type.name.camelCase() }}GetHandle)) : nullptr;
                {% endif %}
            {% endif %}
        {%- endfor %}
        {{ as_cType(structure.name) }} converted = {
            {% if len(structure.chain_roots) == 1 %}
                .chain = {.sType = WGPUSType_{{ structure.name.CamelCase() }}},
            {% endif %}
            {% for member in structure.members %}
                {%- if member.length.name %}
                    .{{ member.length.name.camelCase() }}{{ ' = ' }}
                    {%- if member.type.category == 'object' %}
                        {{ member.length.name.camelCase() }}
                    {%- else %}
                        static_cast<{{ as_cType(member.length.type.name) }}>(
                                native{{ member.name.CamelCase() }} ? env->GetArrayLength(
                                static_cast<jarray>(native{{ member.name.CamelCase() }})) : 0)
                    {%- endif %},
                {%- endif %}
                .{{ member.name.camelCase() }}{{ ' = ' -}}
                {%- if member.annotation == 'const*' %}
                    {%- if member.type.name.get() == 'char' -%}
                        getString(env, env->CallObjectMethod(obj, env->GetMethodID(
                                {{ structure.name.camelCase() }}Class,
                                "get{{ member.name.CamelCase() }}", "()Ljava/lang/String;")))
                    {%- elif member.type.category == 'object' -%}
                        native{{ member.name.CamelCase() }}
                    {%- elif member.type.name.get() == 'float' -%}
                        env->GetFloatArrayElements(static_cast<jfloatArray>(
                                native{{ member.name.CamelCase() }}), 0)
                    {%- elif member.type.name.get() == 'uint32' -%}
                        env->GetIntArrayElements(static_cast<jintArray>(
                                native{{ member.name.CamelCase() }}), 0)
                    {%- elif member.type.category == 'structure' -%}
                        {%- if member.length == 'constant' -%}
                            convert{{ member.type.name.CamelCase() }}Optional(env,
                                    env->CallObjectMethod(obj,
                                    env->GetMethodID({{ structure.name.camelCase() }}Class,
                                    "get{{ member.name.CamelCase() }}",
                                    "()L{{ metadata.kotlin_path }}/{{ member.type.name.CamelCase() }};")))
                        {%- else -%}
                            convert{{ member.type.name.CamelCase() }}Array(env,
                                    static_cast<jobjectArray>(native{{ member.name.CamelCase() }}))
                        {%- endif -%}
                    {%- elif member.type.category in ['bitmask', 'enum'] -%}
                        reinterpret_cast<{{ as_cType(member.type.name) }}*>(
                                native{{ member.name.CamelCase() }})
                    {%- else -%}
                        nullptr
                    {%- endif -%}
                {%- elif member.type.name.get() == 'bool' -%}
                    env->CallBooleanMethod(obj, env->GetMethodID(
                            {{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()Z"))
                {%- elif member.type.name.get() == 'double' -%}
                    env->CallDoubleMethod(obj, env->GetMethodID(
                            {{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()D"))
                {%- elif member.type.name.get() == 'float' -%}
                    env->CallFloatMethod(obj, env->GetMethodID(
                            {{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()F"))
                {%- elif member.type.name.get() == 'uint16_t' -%}
                    static_cast<uint16_t>(env->CallShortMethod(obj, env->GetMethodID(
                            {{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()S")))
                {%- elif member.type.name.get() == 'int' -%}
                    env->CallIntMethod(obj, env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()I"))
                {%- elif member.type.name.get() == 'int32_t' -%}
                    static_cast<int32_t>(env->CallIntMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.name.get() == 'uint32_t' -%}
                    static_cast<uint32_t>(env->CallIntMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.name.get() == 'uint64_t' -%}
                    static_cast<uint64_t>(env->CallLongMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()J")))
                {%- elif member.type.name.get() == 'size_t' -%}
                    static_cast<size_t>(env->CallLongMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()J")))
                {%- elif member.type.category in ['bitmask', 'enum'] -%}
                    static_cast<{{ as_cType(member.type.name) }}>(env->CallIntMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()I")))
                {%- elif member.type.category == 'object' -%}
                    native{{ member.name.CamelCase() }}
                {%- elif member.type.category == 'structure' -%}
                    convert{{ member.type.name.CamelCase() }}(env,env->CallObjectMethod(obj,
                            env->GetMethodID({{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}",
                            "()L{{ metadata.kotlin_path }}/{{ member.type.name.CamelCase() }};")))
                {%- elif structure.name.get() == 'surface descriptor from android native window'
                        and member.name.get() == 'window' -%}
                    reinterpret_cast<void*>(env->CallLongMethod(obj, env->GetMethodID(
                            {{ structure.name.camelCase() }}Class,
                            "get{{ member.name.CamelCase() }}", "()J")))
                {%- else -%}
                    nullptr
                {%- endif %}
            {{- ',' if not loop.last }}
            {% endfor %}
        };

        {% for child in chain_children[structure.name.get()] %}
            jobject {{ child.name.camelCase() }} = env->CallObjectMethod(
                    obj, env->GetMethodID({{ structure.name.camelCase() }}Class,
                    "get{{ child.name.CamelCase() }}",
                    "()L{{ metadata.kotlin_path }}/{{ child.name.CamelCase() }};"));
            if ({{ child.name.camelCase() }}) {
              WGPUChainedStruct * convertedChild =
                    &(new {{ as_cType(child.name) }}(convert{{ child.name.CamelCase() }}(
                    env, {{ child.name.camelCase() }})))->chain;
              convertedChild->next = converted.nextInChain;
              converted.nextInChain = convertedChild;
            }
        {% endfor %}
        return converted;
    }

    const {{ as_cType(structure.name) }}* convert{{ structure.name.CamelCase() }}Optional(
            JNIEnv *env, jobject obj) {
        if (!obj) {
            return nullptr;
        }
        return new {{ as_cType(structure.name) }}(
                convert{{ structure.name.CamelCase() }}(env, obj));
    }

    const {{ as_cType(structure.name) }}* convert{{ structure.name.CamelCase() }}Array(
            JNIEnv *env, jobjectArray arr) {
        if (!arr) {
            return nullptr;
        }
        jsize len = env->GetArrayLength(arr);
        {{ as_cType(structure.name) }}* entries = new {{ as_cType(structure.name) }}[len];
        for (int idx = 0; idx != len; idx++) {
            entries[idx] = convert{{ structure.name.CamelCase() }}(
                    env, env->GetObjectArrayElement(arr, idx));
        }
        return entries;
    }

{% endfor %}
