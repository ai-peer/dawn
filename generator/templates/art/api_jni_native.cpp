//* Copyright 2024 The Dawn & Tint Authors
//*
//* Redistribution and use in source and binary forms, with or without
//* modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this
//*    list of conditions and the following disclaimer.
//*
//* 2. Redistributions in binary form must reproduce the above copyright notice,
//*    this list of conditions and the following disclaimer in the documentation
//*    and/or other materials provided with the distribution.
//*
//* 3. Neither the name of the copyright holder nor the names of its
//*    contributors may be used to endorse or promote products derived from
//*    this software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <jni.h>
#include "dawn/webgpu.h"
#include "Converter.h"

// Converts every method call from the Kotlin-hosted version to the native version, performing
// the necessary JNI calls to fetch data, and converting the data to the correct form.

#define DEFAULT __attribute__((visibility("default")))

struct UserData {
    JNIEnv *env;
    jobject callback;
};

{% macro render_method(method, object) %}
    //*  A JNI-external method is built with the JNI signature expected to match the host Kotlin.
    DEFAULT extern "C"
    {% if method.return_type.category == 'structure' %}
        {{ unreachable_code() }}   //*  Currently returning structures is not supported.
    {% elif method.return_type.category in ['bitmask', 'enum'] -%}
        jint
    {%- elif method.return_type.category == 'object' -%}
        jobject
    {%- else -%}
        {{ to_jni_type[method.return_type.name.get()] }}
    {%- endif %}
    {{ ' ' }}
    Java_{{ kotlin_package.replace('.', '_') }}_
            {{- object.name.CamelCase() if object else 'FunctionsKt' -}}
            _{{ method.name.camelCase() }}(JNIEnv *env
                    {{ ', jobject obj' if object else ', jclass clazz' -}}

    {% set input_args = [] %}

    //* Make the signature for each argument in turn, ignoring userdata and length arguments which
    //* are not present in the Kotlin host method.
    {% for arg in method.arguments if arg.name.get() != 'userdata'
            and not method.arguments | selectattr('length', 'equalto', arg) | first %},
        {% do input_args.append(arg) %}
        {%- if arg.length == 'strlen' -%}
            jstring
        {%- elif arg.length and arg.constant_length != 1 %}
            {%- if arg.type.category in [
                    'bitmask', 'enum', 'function pointer', 'object', 'structure'] -%}
                jobjectArray
            {%- elif arg.type.name.get() == 'void' -%}
                jobject
            {%- elif arg.type.name.get() == 'uint32_t' -%}
                jintArray
            {%- else %}
                //* Currently returning structures in callbacks is not supported.
                {{ unreachable_code() }}
            {% endif %}
        {%- elif arg.type.category in ['function pointer', 'object', 'structure'] -%}
            jobject
        {%- elif arg.type.category in ['bitmask', 'enum'] -%}
            jint
        {%- elif arg.type.name.get() in ['float',  'int32_t', 'uint32_t', 'uint64_t', 'size_t'] -%}
            {{ to_jni_type[arg.type.name.get()] }}
        {%- else %}
            {{ unreachable_code() }}
        {% endif %}
        {{ ' ' }}_{{ arg.name.camelCase() }}
    {% endfor %}) {

    //*  A variable is declared for each parameter of the native method.
    {% for arg in method.arguments %}
        {{ as_annotated_cType(arg) }};
    {% endfor %}

    //* Each parameter is converted from the JNI parameter to the expected form of the native
    //* parameter.
    {% for arg in input_args %}
        {% if arg.length == 'strlen' %}
            if (_{{ arg.name.camelCase() }}) {
                {{ arg.name.camelCase() }} = env->GetStringUTFChars(_{{ arg.name.camelCase() }}, 0);
            }
        {% elif arg.constant_length == 1 %}
            //*  Optional structure.
            {% if arg.type.category == 'structure' %}
                if (_{{ arg.name.camelCase() }}) {
                    auto convertedMember = new {{ as_cType(arg.type.name) }}();
                    convert(env, _{{ arg.name.camelCase() }}, convertedMember);
                    {{ arg.name.camelCase() }} = convertedMember;
                } else {
                    {{ arg.name.camelCase() }} = nullptr;
                }
            {% else %}
                {{ unreachable_code() }}
            {% endif %}
        {% elif arg.length %}
            //*  Container types.
            {% if arg.type.name.get() == 'uint32_t' %}
                {{ arg.name.camelCase() }} =
                        reinterpret_cast<{{ as_cType(arg.type.name) }}*>(
                               env->GetIntArrayElements(_{{ arg.name.camelCase() }}, 0));
               {{ arg.length.name.camelCase() }} =
                       env->GetArrayLength(_{{ arg.name.camelCase() }});
            {% elif arg.type.name.get() == 'void' %}
                {{ arg.name.camelCase() }} =
                        env->GetDirectBufferAddress(_{{ arg.name.camelCase() }});
                {{ arg.length.name.camelCase() }} =
                        env->GetDirectBufferCapacity(_{{ arg.name.camelCase() }});
            {% else %} {
                size_t length = env->GetArrayLength(_{{ arg.name.camelCase() }});
                auto out = new {{ as_cType(arg.type.name) }}[length]();
                {% if arg.type.category in ['bitmask', 'enum'] %} {
                    jclass memberClass = env->FindClass("{{ jni_name(arg.type) }}");
                    jmethodID getValue = env->GetMethodID(memberClass, "getValue", "()I");
                    for (int idx = 0; idx != length; idx++) {
                        jobject element =
                                env->GetObjectArrayElement(_{{ arg.name.camelCase() }}, idx);
                        out[idx] = static_cast<{{ as_cType(arg.type.name) }}>(
                                env->CallIntMethod(element, getValue));
                    }
                }
                {% elif arg.type.category == 'object' %} {
                    jclass memberClass = env->FindClass("{{ jni_name(arg.type) }}");
                    jmethodID getHandle = env->GetMethodID(memberClass, "getHandle", "()J");
                    for (int idx = 0; idx != length; idx++) {
                        jobject element =
                                env->GetObjectArrayElement(_{{ arg.name.camelCase() }}, idx);
                        out[idx] = reinterpret_cast<{{ as_cType(arg.type.name) }}>(
                                env->CallLongMethod(element, getHandle));
                    }
                }
                {% else %}
                    {{ unreachable_code() }}
                {% endif %}
                {{ arg.name.camelCase() }} = out;
                {{ arg.length.name.camelCase() }} = length;
            }
            {% endif %}

        //*  Single value types.
        {% elif arg.type.category == 'object' %}
            if (_{{ arg.name.camelCase() }}) {
                jclass memberClass = env->FindClass("{{ jni_name(arg.type) }}");
                jmethodID getHandle = env->GetMethodID(memberClass, "getHandle", "()J");
                {{ arg.name.camelCase() }} =
                        reinterpret_cast<{{ as_cType(arg.type.name) }}>(
                                env->CallLongMethod(_{{ arg.name.camelCase() }}, getHandle));
            }
        {% elif arg.type.name.get() in ['int', 'int32_t', 'uint32_t']
                or arg.type.category in ['bitmask', 'enum'] %}
            {{ arg.name.camelCase() }} =
                    static_cast<{{ as_cType(arg.type.name) }}>(_{{ arg.name.camelCase() }});
        {% elif arg.type.name.get() == 'float' %}
            {{ arg.name.camelCase() }} = _{{ arg.name.camelCase() }};
        {% elif arg.type.name.get() in ['size_t', 'uint64_t'] %}
            {{ arg.name.camelCase() }} =
                    static_cast<{{ as_cType(arg.type.name) }}>(_{{ arg.name.camelCase() }});

        {% elif arg.type.category == 'function pointer' %} {
            //* Function pointers themselves require each argument converting.
            //* A custom native callback is generated to wrap the ART callback.
            {{ arg.name.camelCase() }} = [](
                {%- for callbackArg in arg.type.arguments %}
                    {{ as_annotated_cType(callbackArg) }}{{ ',' if not loop.last }}
                {%- endfor %}) {
                UserData* userData1 = static_cast<UserData *>(userdata);
                JNIEnv *env = userData1->env;
                if (env->ExceptionCheck()) {
                    return;
                }

                //* Get the client (ART) callback so we can call it.
                jmethodID callbackMethod = env->GetMethodID(
                        env->FindClass("{{ jni_name(arg.type) }}"), "callback", "(
                    {%- for callbackArg in arg.type.arguments if
                            callbackArg.name.get() != 'userdata'-%}
                        {%- if callbackArg.type.category in ['bitmask', 'enum'] -%}
                            I
                        {%- elif callbackArg.type.category in ['object', 'structure'] -%}
                            L{{ jni_name(callbackArg.type) }};
                        {%- elif callbackArg.type.name.get() == 'char' -%}
                            Ljava/lang/String;
                        {%- else -%}
                            {{ unreachable_code() }}
                        {%- endif -%}
                    {%- endfor %})V");

                //* Call the callback with all converted parameters.
                env->CallVoidMethod(userData1->callback, callbackMethod
                {%- for callbackArg in arg.type.arguments if callbackArg.name.get() != 'userdata' %}
                    ,
                    {%- if callbackArg.type.category == 'object' %}
                        env->NewObject(env->FindClass("{{ jni_name(callbackArg.type) }}"),
                                env->GetMethodID(env->FindClass("{{ jni_name(callbackArg.type) }}"),
                                        "<init>", "(J)V"),
                                reinterpret_cast<jlong>({{ callbackArg.name.camelCase() }}))
                    {%- elif callbackArg.type.category in ['bitmask', 'enum'] %}
                        static_cast<jint>({{ callbackArg.name.camelCase() }})
                    {%- elif callbackArg.type.category == 'structure' %}
                        {{ unreachable_code () }}  //* We don't yet handle structures in callbacks.
                    {%- elif callbackArg.type.name.get() == 'char' %}
                        env->NewStringUTF({{ callbackArg.name.camelCase() }})
                    {%- else %}
                        {{ callbackArg.name.camelCase() }}
                    {%- endif %}
                {%- endfor %});
            };
            userdata = new UserData(
                    {.env = env, .callback = env->NewGlobalRef(_{{ arg.name.camelCase() }})});
        }
        {% else %}
            {{ unreachable_code() }}
        {% endif %}
    {% endfor %}

    {% if object %}
        jclass memberClass = env->FindClass("{{ jni_name(object) }}");
        jmethodID getHandle = env->GetMethodID(memberClass, "getHandle", "()J");
        auto handle =
                reinterpret_cast<{{ as_cType(object.name) }}>(env->CallLongMethod(obj, getHandle));
    {% endif %}

    //* Actually invoke the native version of the method.
    {{ 'auto result =' if method.return_type.name.get() != 'void' }}
    {% if object %}
        wgpu{{ object.name.CamelCase() }}{{ method.name.CamelCase() }}(handle
    {% else %}
        wgpu{{ method.name.CamelCase() }}(
    {% endif %}
        {% for arg in method.arguments -%}
            {{- ',' if object or not loop.first }}{{ arg.name.camelCase() -}}
        {% endfor %}
    );
    if (env->ExceptionCheck()) {  //* Early out if client (ART) callback threw an exception.
        return{{ ' 0' if method.return_type.name.get() != 'void' }};
    }

    //* We only handle objects and primitives to be returned.
    {% if method.return_type.category == 'object' %}
        jclass returnClass = env->FindClass("{{ jni_name(method.return_type) }}");
        auto constructor = env->GetMethodID(returnClass, "<init>", "(J)V");
        return env->NewObject(returnClass, constructor, reinterpret_cast<jlong>(result));
    {% elif method.return_type.name.get() != 'void' %}
        return result;  //* Primitives are implicitly converted by JNI.
    {% endif %}
}
{% endmacro %}

{% for obj in by_category['object'] %}
    {% for method in obj.methods if include_method(method) %}
        {{ render_method(method, obj) }}
    {% endfor %}
{% endfor %}

//* Global functions don't have an associated class.
{% for function in by_category['function'] if include_method(function) %}
    {{ render_method(function, None) }}
{% endfor %}
