cmake_policy(SET CMP0048 NEW)
project(webgpu VERSION 1.1.0 LANGUAGES CXX)

cmake_minimum_required(VERSION 3.22.1)

include(${CMAKE_CURRENT_LIST_DIR}/BundleLibrary.cmake)

set(DAWN_BUILD_ANDROID_SAMPLES OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)

add_subdirectory("dawn")

bundle_library(webgpu_dawn webgpu_dawn_bundled)

bundle_library(webgpu_cpp webgpu_cpp_bundled)

add_custom_command(TARGET webgpu_cpp_bundled POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${CMAKE_CURRENT_BINARY_DIR}/dawn/gen/include
                           ${CMAKE_SOURCE_DIR}/gen/include)
