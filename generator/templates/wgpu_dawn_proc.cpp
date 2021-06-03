// Copyright 2018 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef WGPU_DAWN_EXPORT_H_
#define WGPU_DAWN_EXPORT_H_

#if defined(WGPU_DAWN_SHARED_LIBRARY)
#    if defined(_WIN32)
#        if defined(WGPU_DAWN_IMPLEMENTATION)
#            define WGPU_DAWN_EXPORT __declspec(dllexport)
#        else
#            define WGPU_DAWN_EXPORT __declspec(dllimport)
#        endif
#    else  // defined(_WIN32)
#        if defined(WGPU_DAWN_IMPLEMENTATION)
#            define WGPU_DAWN_EXPORT __attribute__((visibility("default")))
#        else
#            define WGPU_DAWN_EXPORT
#        endif
#    endif  // defined(_WIN32)
#else       // defined(WGPU_DAWN_SHARED_LIBRARY)
#    define WGPU_DAWN_EXPORT
#endif  // defined(WGPU_DAWN_SHARED_LIBRARY)

#include <dawn/webgpu_cpp.h>

namespace dawn_native {

{% for type in by_category["object"] %}
    extern WGPUInstance NativeCreateInstance(WGPUInstanceDescriptor const * descriptor);
    extern WGPUProc NativeGetProcAddress(WGPUDevice device, const char* procName);
    {% for method in c_methods(type) %}
        extern {{as_cType(method.return_type.name)}} Native{{as_MethodSuffix(type.name, method.name)}}(
            {{-as_cType(type.name)}} cSelf
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        );
    {% endfor %}
{% endfor %}

}

extern "C" {
    using namespace dawn_native;

    WGPU_DAWN_EXPORT WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
        return NativeCreateInstance(descriptor);
    }

    WGPU_DAWN_EXPORT WGPUProc wgpuGetProcAddress(WGPUDevice device, const char* procName) {
        return NativeGetProcAddress(device, procName);
    }

    {% for type in by_category["object"] %}
        {% for method in c_methods(type) %}
            WGPU_DAWN_EXPORT {{as_cType(method.return_type.name)}} wgpu{{as_MethodSuffix(type.name, method.name)}}(
                {{-as_cType(type.name)}} cSelf
                {%- for arg in method.arguments -%}
                    , {{as_annotated_cType(arg)}}
                {%- endfor -%}
            ) {
                return Native{{as_MethodSuffix(type.name, method.name)}}(
                    cSelf
                    {%- for arg in method.arguments -%}
                        , {{as_varName(arg.name)}}
                    {%- endfor -%}
                );
            }
        {% endfor %}
    {% endfor %}
}

#endif  // WGPU_DAWN_EXPORT_H_
