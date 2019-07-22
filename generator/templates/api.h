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

const uint64_t WGPU_WHOLE_SIZE = 0xffffffffffffffffULL; // UINT64_MAX

typedef uint32_t WGPUFlags;
typedef void* WGPUObject;

{% for type in by_category["object"] %}
    typedef struct {{as_cType(type.name)}}Impl* {{as_cType(type.name)}};
{% endfor %}

{% for type in by_category["enum"] + by_category["bitmask"] %}
    typedef enum {{as_cType(type.name)}} {
        {% for value in type.values %}
            {{as_cEnum(type.name, value.name)}} = 0x{{format(value.value, "08X")}},
        {% endfor %}
        {{as_cEnum(type.name, Name("force32"))}} = 0x7FFFFFFF
    } {{as_cType(type.name)}};
    {% if type.category == "bitmask" %}
        typedef WGPUFlags {{as_cType(type.name)}}Flags;
    {% endif %}

{% endfor %}

{% for type in by_category["structure"] %}
    typedef struct {{as_cType(type.name)}} {
        {% if type.extensible %}
            void const * nextInChain;
        {% endif %}
        {% for member in type.members %}
            {{as_annotated_cType(member)}};
        {% endfor %}
    } {{as_cType(type.name)}};

{% endfor %}

typedef void (*WGPUBufferCreateMappedCallback)(WGPUBufferMapAsyncStatus status,
                                               WGPUCreateBufferMappedResult result,
                                               void* userdata);
typedef void (*WGPUBufferMapReadCallback)(WGPUBufferMapAsyncStatus status,
                                          const void* data,
                                          uint64_t dataLength,
                                          void* userdata);
typedef void (*WGPUBufferMapWriteCallback)(WGPUBufferMapAsyncStatus status,
                                           void* data,
                                           uint64_t dataLength,
                                           void* userdata);
typedef void (*WGPUFenceOnCompletionCallback)(WGPUFenceCompletionStatus status, void* userdata);

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WGPUProcReference)(WGPUObject object);
typedef void (*WGPUProcRelease)(WGPUObject object);

{% for type in by_category["object"] if len(native_methods(type)) > 0 %}
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

#if !defined(WGPU_SKIP_DECLARATIONS)

WGPU_EXPORT void wgpuReference(WGPUObject object);
WGPU_EXPORT void wgpuRelease(WGPUObject object);

{% for type in by_category["object"] if len(native_methods(type)) > 0 %}
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

#endif  // !defined(WGPU_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WGPU_WGPU_H_
