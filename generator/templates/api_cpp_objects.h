//* Copyright 2017 The Dawn & Tint Authors
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
{% set API = metadata.api.upper() %}
{% set api = API.lower() %}
{% if 'dawn' in enabled_tags %}
    #ifdef __EMSCRIPTEN__
    #error "Do not include this header. Emscripten already provides headers needed for {{metadata.api}}."
    #endif
{% endif %}

#ifndef {{API}}_CPP_OBJECTS_H_
#define {{API}}_CPP_OBJECTS_H_

#include "{{api}}/{{api}}.h"

namespace {{metadata.namespace}} {
    {% set c_prefix = metadata.c_prefix %}

    template<typename Derived, typename CType>
    class ObjectBase {
      public:
        ObjectBase() = default;
        ObjectBase(CType handle): mHandle(handle) {
            if (mHandle) Derived::{{c_prefix}}Reference(mHandle);
        }
        ~ObjectBase() {
            if (mHandle) Derived::{{c_prefix}}Release(mHandle);
        }

        ObjectBase(ObjectBase const& other)
            : ObjectBase(other.Get()) {
        }
        Derived& operator=(ObjectBase const& other) {
            if (&other != this) {
                if (mHandle) Derived::{{c_prefix}}Release(mHandle);
                mHandle = other.mHandle;
                if (mHandle) Derived::{{c_prefix}}Reference(mHandle);
            }

            return static_cast<Derived&>(*this);
        }

        ObjectBase(ObjectBase&& other) {
            mHandle = other.mHandle;
            other.mHandle = 0;
        }
        Derived& operator=(ObjectBase&& other) {
            if (&other != this) {
                if (mHandle) Derived::{{c_prefix}}Release(mHandle);
                mHandle = other.mHandle;
                other.mHandle = 0;
            }

            return static_cast<Derived&>(*this);
        }

        ObjectBase(std::nullptr_t) {}
        Derived& operator=(std::nullptr_t) {
            if (mHandle != nullptr) {
                Derived::{{c_prefix}}Release(mHandle);
                mHandle = nullptr;
            }
            return static_cast<Derived&>(*this);
        }

        bool operator==(std::nullptr_t) const {
            return mHandle == nullptr;
        }
        bool operator!=(std::nullptr_t) const {
            return mHandle != nullptr;
        }

        explicit operator bool() const {
            return mHandle != nullptr;
        }
        CType Get() const {
            return mHandle;
        }
        CType MoveToCHandle() {
            CType result = mHandle;
            mHandle = 0;
            return result;
        }
        static Derived Acquire(CType handle) {
            Derived result;
            result.mHandle = handle;
            return result;
        }

      protected:
        CType mHandle = nullptr;
    };

{% macro render_cpp_default_value(member, is_struct, force_default=False) -%}
    {%- if member.json_data.get("no_default", false) -%}
    {%- elif member.annotation in ["*", "const*"] and member.optional or member.default_value == "nullptr" -%}
        {{" "}}= nullptr
    {%- elif member.type.category == "object" and member.optional and is_struct -%}
        {{" "}}= nullptr
    {%- elif member.type.category in ["enum", "bitmask"] and member.default_value != None -%}
        {{" "}}= {{as_cppType(member.type.name)}}::{{as_cppEnum(Name(member.default_value))}}
    {%- elif member.type.category == "native" and member.default_value != None -%}
        {{" "}}= {{member.default_value}}
    {%- elif member.default_value != None -%}
        {{" "}}= {{member.default_value}}
    {%- else -%}
        {{assert(member.default_value == None)}}
        {%- if force_default -%}
            {{" "}}= {}
        {%- endif -%}
    {%- endif -%}
{%- endmacro %}

{% macro render_cpp_method_declaration(type, method, dfn=False) %}
    {% set CppType = as_cppType(type.name) %}
    {% set OriginalMethodName = method.name.CamelCase() %}
    {% set MethodName = OriginalMethodName[:-1] if method.name.chunks[-1] == "f" else OriginalMethodName %}
    {% set MethodName = CppType + "::" + MethodName if dfn else MethodName %}
    {{as_cppType(method.return_type.name)}} {{MethodName}}(
        {%- for arg in method.arguments -%}
            {%- if not loop.first %}, {% endif -%}
            {%- if arg.type.category == "object" and arg.annotation == "value" -%}
                {{as_cppType(arg.type.name)}} const& {{as_varName(arg.name)}}
            {%- else -%}
                {{as_annotated_cppType(arg)}}
            {%- endif -%}
            {% if not dfn %}{{render_cpp_default_value(arg, False)}}{% endif %}
        {%- endfor -%}
    ) const
{%- endmacro %}

    {% for type in by_category["function pointer"] %}
        using {{as_cppType(type.name)}} = {{as_cType(type.name)}};
    {% endfor %}

    {% for type in by_category["object"] %}
        class {{as_cppType(type.name)}};
    {% endfor %}

    {% for type in by_category["structure"] %}
        struct {{as_cppType(type.name)}};
    {% endfor %}

    {% for type in by_category["object"] %}
        {% set CppType = as_cppType(type.name) %}
        {% set CType = as_cType(type.name) %}
        class {{CppType}} : public ObjectBase<{{CppType}}, {{CType}}> {
          public:
            using ObjectBase::ObjectBase;
            using ObjectBase::operator=;

            {% for method in type.methods %}
                inline {{render_cpp_method_declaration(type, method)}};
            {% endfor %}

          private:
            friend ObjectBase<{{CppType}}, {{CType}}>;
            static inline void {{c_prefix}}Reference({{CType}} handle);
            static inline void {{c_prefix}}Release({{CType}} handle);
        };

    {% endfor %}

}  // {{metadata.namespace}}

#endif  // {{API}}_CPP_OBJECTS_H_
