# Copyright 2022 The Dawn & Tint Authors
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

# Don't build testing in third_party dependencies
set(BUILD_TESTING OFF)

# fetch_dawn_dependencies.py is an alternative to using depot_tools
# It is particularly interesting when building dawn as a subdirectory in
# a parent project that does not want to use depot_tools.
if (${DAWN_FETCH_DEPENDENCIES})
    find_package(PythonInterp 3 REQUIRED)

    set(EXTRA_FETCH_ARGS)
    if (NOT TARGET gmock AND ${TINT_BUILD_TESTS})
        list(APPEND EXTRA_FETCH_ARGS --use-test-deps)
    endif()

    message(STATUS "Running fetch_dawn_dependencies:")
    execute_process(
        COMMAND
            ${PYTHON_EXECUTABLE}
            "${PROJECT_SOURCE_DIR}/tools/fetch_dawn_dependencies.py"
            --directory ${PROJECT_SOURCE_DIR}
            ${EXTRA_FETCH_ARGS}
    )
endif ()

if (NOT TARGET SPIRV-Headers)
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON)
    set(SPIRV_HEADERS_SKIP_INSTALL ON)

    message(STATUS "Dawn: using SPIRV-Headers at ${DAWN_SPIRV_HEADERS_DIR}")
    add_subdirectory(${DAWN_SPIRV_HEADERS_DIR} "${CMAKE_CURRENT_BINARY_DIR}/spirv-headers")
endif()

# Needs to come before SPIR-V Tools
if ((${TINT_BUILD_SPIRV_TOOLS_FUZZER} OR ${TINT_BUILD_AST_FUZZER}) AND
        (NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc))
    set(protobuf_BUILD_TESTS OFF)
    set(protobuf_MSVC_STATIC_RUNTIME OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/protobuf/cmake)
endif()

if (NOT TARGET SPIRV-Tools)
    set(SPIRV_SKIP_TESTS ON)
    set(SPIRV_SKIP_EXECUTABLES ON)
    set(SKIP_SPIRV_TOOLS_INSTALL ON)

    if(${TINT_BUILD_SPV_READER} OR ${TINT_BUILD_SPV_WRITER})
        set(SPIRV_SKIP_TESTS ON)
        set(SPIRV_WERROR OFF)
        if (${TINT_BUILD_SPIRV_TOOLS_FUZZER})
            set(SPIRV_BUILD_FUZZER ON)
        endif()
    endif()

    message(STATUS "Dawn: using SPIRV-Tools at ${DAWN_SPIRV_TOOLS_DIR}")
    add_subdirectory(${DAWN_SPIRV_TOOLS_DIR} "${CMAKE_CURRENT_BINARY_DIR}/spirv-tools" EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET glslang AND ${TINT_BUILD_GLSL_WRITER} AND ${TINT_BUILD_SAMPLES})
    set(SKIP_GLSLANG_INSTALL ON)
    add_subdirectory("${DAWN_THIRD_PARTY_DIR}/vulkan-deps/glslang/src" "${CMAKE_CURRENT_BINARY_DIR}/glslang" EXCLUDE_FROM_ALL)
endif()

if (NOT TARGET glfw AND DAWN_ENABLE_WINDOWING_GLFW)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_X11 ${DAWN_USE_X11})
    set(GLFW_BUILD_WAYLAND ${DAWN_USE_WAYLAND})

    message(STATUS "Dawn: using GLFW at ${DAWN_GLFW_DIR}")
    add_subdirectory(${DAWN_GLFW_DIR} "${CMAKE_CURRENT_BINARY_DIR}/glfw")
endif()

if (NOT TARGET libabsl)
    message(STATUS "Dawn: using Abseil at ${DAWN_ABSEIL_DIR}")
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR
        ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"))
        add_compile_options(
            -Wno-array-parameter
            -Wno-deprecated-builtins
            -Wno-unknown-warning-option
        )
    endif()

    add_subdirectory(${DAWN_ABSEIL_DIR} "${CMAKE_CURRENT_BINARY_DIR}/abseil")
endif()

if (NOT TARGET Vulkan-Headers)
    message(STATUS "Dawn: using Vulkan-Headers at ${DAWN_VULKAN_HEADERS_DIR}")
    add_subdirectory(${DAWN_VULKAN_HEADERS_DIR} "${CMAKE_CURRENT_BINARY_DIR}/vulkan-headers")
endif()

# Header-only library for khrplatform.h
add_library(dawn_khronos_platform INTERFACE)
target_sources(dawn_khronos_platform INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/khronos/KHR/khrplatform.h")
target_include_directories(dawn_khronos_platform INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/khronos")

# Header-only library for Vulkan headers
add_library(dawn_vulkan_headers INTERFACE)
target_sources(dawn_vulkan_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vk_icd.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vk_layer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vk_platform.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vk_sdk_platform.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vulkan.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/khronos/vulkan/vulkan_core.h"
)
target_include_directories(dawn_vulkan_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/khronos")

if (NOT TARGET vk_swiftshader AND ${DAWN_ENABLE_SWIFTSHADER})
    set(SWIFTSHADER_BUILD_TESTS OFF)
    set(SWIFTSHADER_BUILD_BENCHMARKS OFF)

    message(STATUS "Dawn: using Swiftshader at ${DAWN_SWIFTSHADER_DIR}")
    add_subdirectory(${DAWN_SWIFTSHADER_DIR} "${CMAKE_CURRENT_BINARY_DIR}/swiftshader")
endif()

if (${TINT_BUILD_BENCHMARKS} OR ${DAWN_BUILD_BENCHMARKS})
    set(BENCHMARK_ENABLE_TESTING OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/google_benchmark/src EXCLUDE_FROM_ALL)
endif()

if (NOT TARGET gmock AND ${TINT_BUILD_TESTS})
    set(gtest_force_shared_crt ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest EXCLUDE_FROM_ALL)
endif()
