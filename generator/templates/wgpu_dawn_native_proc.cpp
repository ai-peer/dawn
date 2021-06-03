// Copyright 2021 The Dawn Authors
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

#include <dawn/webgpu.h>

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

    WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
        return NativeCreateInstance(descriptor);
    }

    WGPUProc wgpuGetProcAddress(WGPUDevice device, const char* procName) {
        return NativeGetProcAddress(device, procName);
    }

    {% for type in by_category["object"] %}
        {% for method in c_methods(type) %}
            {{as_cType(method.return_type.name)}} wgpu{{as_MethodSuffix(type.name, method.name)}}(
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
