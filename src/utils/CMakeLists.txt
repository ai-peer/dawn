# Copyright 2020 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_library(dawn_utils STATIC)
target_sources(dawn_utils
    PUBLIC
        "BackendBinding.h"
        "ComboRenderBundleEncoderDescriptor.h"
        "ComboRenderPipelineDescriptor.h"
        "GLFWUtils.h"
        "SystemUtils.h"
        "TerribleCommandBuffer.h"
        "Timer.h"
        "WGPUHelpers.h"
    PRIVATE
        "BackendBinding.cpp"
        "ComboRenderBundleEncoderDescriptor.cpp"
        "ComboRenderPipelineDescriptor.cpp"
        "GLFWUtils.cpp"
        "SystemUtils.cpp"
        "TerribleCommandBuffer.cpp"
        "WGPUHelpers.cpp"
)
target_link_libraries(dawn_utils
    PUBLIC dawncpp_headers
    PRIVATE dawn_internal_config
            dawn_common
            dawn_native
            dawn_wire
            shaderc
            glfw
)

if(WIN32)
    target_sources(dawn_utils PRIVATE "WindowsTimer.cpp")
elseif(APPLE)
    target_sources(dawn_utils PRIVATE
        "OSXTimer.cpp"
        "ObjCUtils.h"
        "ObjCUtils.mm"
    )
    target_link_libraries(dawn_utils PRIVATE "-framework QuartzCore")
elseif(UNIX)
    target_sources(dawn_utils PRIVATE "PosixTimer.cpp")
endif()

if (DAWN_ENABLE_D3D12)
    target_sources(dawn_utils PRIVATE "D3D12Binding.cpp")
endif()

if (DAWN_ENABLE_METAL)
    target_sources(dawn_utils PRIVATE
        "GLFWUtils_metal.mm"
        "MetalBinding.mm"
    )
    target_link_libraries(dawn_utils PRIVATE "-framework Metal")
endif()

if (DAWN_ENABLE_NULL)
    target_sources(dawn_utils PRIVATE "NullBinding.cpp")
endif()

if (DAWN_ENABLE_OPENGL)
    target_sources(dawn_utils PRIVATE "OpenGLBinding.cpp")
endif()

if (DAWN_ENABLE_VULKAN)
    target_sources(dawn_utils PRIVATE "VulkanBinding.cpp")
endif()
