{% set api = metadata.proc_table_prefix %}
#include "dawn/{{api.lower()}}_thread_dispatch_proc.h"

#include <thread>

static {{api}}ProcTable nullProcs;
thread_local {{api}}ProcTable perThreadProcs;

void {{api.lower()}}ProcSetPerThreadProcs(const {{api}}ProcTable* procs) {
    if (procs) {
        perThreadProcs = *procs;
    } else {
        perThreadProcs = nullProcs;
    }
}

static WGPUProc ThreadDispatchGetProcAddress(WGPUDevice device, const char* procName) {
    return perThreadProcs.getProcAddress(device, procName);
}

static WGPUInstance ThreadDispatchCreateInstance(WGPUInstanceDescriptor const * descriptor) {
    return perThreadProcs.createInstance(descriptor);
}

{% for type in by_category["object"] %}
    {% for method in c_methods(type) %}
        static {{as_cType(method.return_type.name)}} ThreadDispatch{{as_MethodSuffix(type.name, method.name)}}(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        ) {
            {% if method.return_type.name.canonical_case() != "void" %}return {% endif %}
            perThreadProcs.{{as_varName(type.name, method.name)}}({{as_varName(type.name)}}
                {%- for arg in method.arguments -%}
                    , {{as_varName(arg.name)}}
                {%- endfor -%}
            );
        }
    {% endfor %}
{% endfor %}

extern "C" {
    {{api}}ProcTable {{api.lower()}}ThreadDispatchProcTable = {
        {% for function in by_category["function"] %}
            ThreadDispatch{{as_cppType(function.name)}},
        {% endfor %}
        {% for type in by_category["object"] %}
            {% for method in c_methods(type) %}
                ThreadDispatch{{as_MethodSuffix(type.name, method.name)}},
            {% endfor %}
        {% endfor %}
    };
}
