cmake_policy(SET CMP0048 NEW)
project(webgpu VERSION 1.1.0 LANGUAGES CXX)

cmake_minimum_required(VERSION 3.22.1)

include(${CMAKE_CURRENT_LIST_DIR}/BundleLibraries.cmake)

set(DAWN_BUILD_ANDROID_SAMPLES OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(../../../../../.. dawn)

target_compile_definitions(webgpu_dawn PRIVATE WGPU_IMPLEMENTATION WGPU_SHARED_LIBRARY)

# It is not clear why abs::raw_hash_set is not part of the bundled dependency tree naturally.
bundle_libraries(webgpu_c_bundled webgpu_dawn absl::raw_hash_set)

find_library(log-lib log)

target_link_libraries(webgpu_c_bundled ${log-lib})

add_custom_command(TARGET webgpu_c_bundled POST_BUILD
                   COMMAND rm -r "${CMAKE_SOURCE_DIR}/gen" || echo)

add_custom_command(TARGET webgpu_c_bundled POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                           ${CMAKE_CURRENT_BINARY_DIR}/dawn/gen/include/dawn/webgpu.h
                           ${CMAKE_SOURCE_DIR}/gen/include/dawn/webgpu.h)
