#ifndef WGPU_WGPU_H_
#define WGPU_WGPU_H_

#if !defined(WGPU_EXPORT)
#    if defined(_WIN32)
#        if defined(WGPU_IMPLEMENTATION)
#            define WGPU_EXPORT __declspec(dllexport)
#        else
#            define WGPU_EXPORT __declspec(dllimport)
#        endif
#    else  // defined(_WIN32)
#        if defined(WGPU_IMPLEMENTATION)
#            define WGPU_EXPORT __attribute__((visibility("default")))
#        else
#            define WGPU_EXPORT
#        endif
#    endif  // defined(_WIN32)
#endif  // defined(WGPU_EXPORT)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

{% for type in by_category["object"] %}
    typedef struct {{as_cType(type.name)}}Impl* {{as_cType(type.name)}};
{% endfor %}

{% for type in by_category["enum"] + by_category["bitmask"] %}
    typedef enum {
        {% for value in type.values %}
            {{as_cEnum(type.name, value.name)}} = 0x{{format(value.value, "08X")}},
        {% endfor %}
        {{as_cEnum(type.name, Name("force32"))}} = 0x7FFFFFFF
    } {{as_cType(type.name)}};

{% endfor %}

{% for type in by_category["structure"] %}
    typedef struct {{as_cType(type.name)}} {
        {% if type.extensible %}
            void const * nextInChain;
            wgpuStructureType structureType;
        {% endif %}
        {% for member in type.members %}
            {{as_annotated_cType(member)}};
        {% endfor %}
    } {{as_cType(type.name)}};

{% endfor %}

typedef void (*wgpuBufferCreateMappedCallback)(wgpuBufferMapAsyncStatus status,
                                               wgpuCreateBufferMappedResult result,
                                               void* userdata);
typedef void (*wgpuBufferMapReadCallback)(wgpuBufferMapAsyncStatus status,
                                          const void* data,
                                          uint64_t dataLength,
                                          void* userdata);
typedef void (*wgpuBufferMapWriteCallback)(wgpuBufferMapAsyncStatus status,
                                           void* data,
                                           uint64_t dataLength,
                                           void* userdata);
typedef void (*wgpuFenceOnCompletionCallback)(wgpuFenceCompletionStatus status, void* userdata);

#ifdef __cplusplus
extern "C" {
#endif

{% for type in by_category["object"] %}
    // Procs of {{type.name.CamelCase()}}
    {% for method in native_methods(type) %}
        typedef {{as_cType(method.return_type.name)}} (*{{as_cProc(type.name, method.name)}})(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}

{% endfor %}

struct wgpuProcTable_s {
    {% for type in by_category["object"] %}
        {% for method in native_methods(type) %}
            {{as_cProc(type.name, method.name)}} {{as_varName(type.name, method.name)}};
        {% endfor %}

    {% endfor %}
};
typedef struct wgpuProcTable_s wgpuProcTable;

// Stuff below is for convenience and will forward calls to a static wgpuProcTable.

// Set which wgpuProcTable will be used
WGPU_EXPORT void wgpuSetProcs(const wgpuProcTable* procs);

{% for type in by_category["object"] %}
    // Methods of {{type.name.CamelCase()}}
    {% for method in native_methods(type) %}
        WGPU_EXPORT {{as_cType(method.return_type.name)}} {{as_cMethod(type.name, method.name)}}(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}

{% endfor %}
#ifdef __cplusplus
} // extern "C"
#endif

#endif // WGPU_WGPU_H_
