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
//*
//*
{% set API = metadata.api.upper() %}
{% set api = API.lower() %}
#ifndef DAWN_WIRE_CLIENT_{{API}}_CPP_H_
#define DAWN_WIRE_CLIENT_{{API}}_CPP_H_

#include "dawn/wire/client/{{api}}.h"

{% for function in by_category["function"] %}
    #define {{as_cMethod(None, function.name)}} {{as_cMethod(None, function.name, 'DawnWireClient')}}
{% endfor %}

{% for type in by_category["object"] if len(c_methods(type)) > 0 %}
    {% for method in c_methods(type) %}
        #define {{as_cMethod(type.name, method.name)}} {{as_cMethod(type.name, method.name, 'DawnWireClient')}}
    {% endfor %}
{% endfor %}

#ifdef {{API}}_CPP_H_
#error "{{api}}_cpp.h must not be included before dawn/wire/client/{{api}}_cpp.h"
#endif
#include "{{api}}/{{api}}_cpp.h"

{% for function in by_category["function"] %}
    #undef {{as_cMethod(None, function.name)}}
{% endfor %}

{% for type in by_category["object"] if len(c_methods(type)) > 0 %}
    {% for method in c_methods(type) %}
        #undef {{as_cMethod(type.name, method.name)}}
    {% endfor %}
{% endfor %}

#endif  // DAWN_WIRE_CLIENT_{{API}}_CPP_H_
