# Copyright 2020 The Tint Authors.
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

function(tint_spvtools_compile_options TARGET)
  # We'll use the optimizer for its nice SPIR-V in-memory representation
  target_link_libraries(${TARGET} SPIRV-Tools-opt SPIRV-Tools)

  # We'll be cheating: using internal interfaces to the SPIRV-Tools optimizer.
  target_include_directories(${TARGET} PRIVATE
    ${spirv-tools_SOURCE_DIR}
    ${spirv-tools_BINARY_DIR}
  )
endfunction()

if (TINT_ENABLE_BREAK_IN_DEBUGGER)
  set_source_files_properties(utils/debug/debugger.cc
    PROPERTIES COMPILE_DEFINITIONS "TINT_ENABLE_BREAK_IN_DEBUGGER=1" )
endif()

if(MSVC)
  list(APPEND TINT_LIB_SRCS
    tint.natvis
  )
endif()

# Include the generated CMake tint targets
include("BUILD.cmake")


# If we're building on mac / ios and we have CoreGraphics, then we can use the
# metal API to validate our shaders. This is roughly 4x faster than invoking
# the metal shader compiler executable.
if(APPLE)
  find_library(LIB_CORE_GRAPHICS CoreGraphics)
  if(LIB_CORE_GRAPHICS)
    target_sources(tint_val PRIVATE "lang/msl/validate/msl_metal.mm")
    target_compile_definitions(tint_val PUBLIC "-DTINT_ENABLE_MSL_VALIDATION_USING_METAL_API=1")
    target_compile_options(tint_val PRIVATE "-fmodules" "-fcxx-modules")
    target_link_options(tint_val PUBLIC "-framework" "CoreGraphics")
  endif()
endif()

## Tint library
add_library(libtint
  "tint.cc"
)
target_link_libraries(libtint
  "lib_tint_lang_core_constant"
  "lib_tint_lang_core_type"
  "lib_tint_lang_wgsl_ast_transform"
  "lib_tint_lang_wgsl_inspector"
  "lib_tint_lang_wgsl_sem"
  "lib_tint_utils_strconv"
  "lib_tint_utils_text"
)
tint_default_compile_options(libtint)
target_link_libraries(libtint absl_strings)
set_target_properties(libtint PROPERTIES OUTPUT_NAME "tint")

# if (${TINT_BUILD_FUZZERS})
#   # Tint library with fuzzer instrumentation
#   add_library(libtint-fuzz)
#   tint_default_compile_options(libtint-fuzz)
#   target_link_libraries(libtint-fuzz absl_strings)
#   if (${COMPILER_IS_LIKE_GNU})
#     target_compile_options(libtint-fuzz PRIVATE -fvisibility=hidden)
#   endif()
#
#   if (NOT ${TINT_LIB_FUZZING_ENGINE_LINK_OPTIONS} STREQUAL "")
#     # This is set when the fuzzers are being built by OSS-Fuzz. In this case the
#     # variable provides the necessary linker flags, and OSS-Fuzz will take care
#     # of passing suitable compiler flags.
#     target_link_options(libtint-fuzz PUBLIC ${TINT_LIB_FUZZING_ENGINE_LINK_OPTIONS})
#   else()
#     # When the fuzzers are being built outside of OSS-Fuzz, specific libFuzzer
#     # arguments to enable fuzzing are used.
#     target_compile_options(libtint-fuzz PUBLIC -fsanitize=fuzzer -fsanitize-coverage=trace-cmp)
#     target_link_options(libtint-fuzz PUBLIC -fsanitize=fuzzer -fsanitize-coverage=trace-cmp)
#   endif()
# endif()
#
# if(${TINT_BUILD_SPV_READER} OR ${TINT_BUILD_SPV_WRITER})
#   tint_spvtools_compile_options(libtint)
#   if (${TINT_BUILD_FUZZERS})
#     tint_spvtools_compile_options(libtint-fuzz)
#   endif()
# endif()


################################################################################
# Tests
################################################################################
if(TINT_BUILD_TESTS)
  set(TINT_TEST_SRCS
    "test_main.cc"
  )

  if (${TINT_BUILD_FUZZERS})
    list(APPEND TINT_TEST_SRCS
      fuzzers/mersenne_twister_engine.cc
      fuzzers/mersenne_twister_engine.h
      fuzzers/random_generator.cc
      fuzzers/random_generator.h
      fuzzers/random_generator_engine.cc
      fuzzers/random_generator_engine.h
      fuzzers/random_generator_test.cc
    )
  endif()

  add_executable(tint_unittests ${TINT_TEST_SRCS})

  target_link_libraries(tint_unittests
    "tint_cmd_common_tests"
    "tint_lang_core_builtin_tests"
    "tint_lang_core_constant_tests"
    "tint_lang_core_ir_tests"
    "tint_lang_core_ir_transform_tests"
    "tint_lang_core_type_tests"
    "tint_lang_wgsl_ast_tests"
    "tint_lang_wgsl_ast_transform_tests"
    "tint_lang_wgsl_ast_transform_utils_tests"
    "tint_lang_wgsl_helpers_tests"
    "tint_lang_wgsl_inspector_tests"
    "tint_lang_wgsl_program_tests"
    "tint_lang_wgsl_resolver_tests"
    "tint_lang_wgsl_sem_tests"
    "tint_utils_cli_tests"
    "tint_utils_command_tests"
    "tint_utils_containers_tests"
    "tint_utils_diagnostic_tests"
    "tint_utils_file_tests"
    "tint_utils_ice_tests"
    "tint_utils_macros_tests"
    "tint_utils_math_tests"
    "tint_utils_memory_tests"
    "tint_utils_reflection_tests"
    "tint_utils_result_tests"
    "tint_utils_rtti_tests"
    "tint_utils_strconv_tests"
    "tint_utils_symbol_tests"
    "tint_utils_templates_tests"
    "tint_utils_text_tests"
    "tint_utils_traits_tests"
  )

  set_target_properties(${target} PROPERTIES FOLDER "Tests")

  if(MSVC)
    # TODO(crbug.com/tint/1749): MSVC debug builds can suffer from stack
    # overflows when resolving deeply nested expression chains or statements.
    # Production builds neither use MSVC nor debug, so just bump the stack size
    # for this build combination.
    if (IS_DEBUG_BUILD)
      target_link_options(tint_unittests PRIVATE "/STACK:4194304") # 4MB, default is 1MB
    endif()
  else()
    target_compile_options(tint_unittests PRIVATE
      -Wno-global-constructors
      -Wno-weak-vtables
    )
  endif()

  ## Test executable
  target_include_directories(
      tint_unittests PRIVATE ${gmock_SOURCE_DIR}/include)
  target_link_libraries(tint_unittests libtint gmock tint_utils_io)
  tint_default_compile_options(tint_unittests)

  if(${TINT_BUILD_SPV_READER} OR ${TINT_BUILD_SPV_WRITER})
    tint_spvtools_compile_options(tint_unittests)
  endif()

  add_test(NAME tint_unittests COMMAND tint_unittests)
endif(TINT_BUILD_TESTS)

################################################################################
# Benchmarks
################################################################################
if(TINT_BUILD_BENCHMARKS)
  if(NOT TINT_BUILD_WGSL_READER)
    message(FATAL_ERROR "TINT_BUILD_BENCHMARKS requires TINT_BUILD_WGSL_READER")
  endif()

  list(APPEND TINT_BENCHMARK_SRCS
    "utils/rtti/switch_bench.cc"
    "bench/benchmark.cc"
    "lang/wgsl/reader/reader_bench.cc"
  )

  if (${TINT_BUILD_GLSL_WRITER})
    list(APPEND TINT_BENCHMARK_SRCS lang/glsl/writer/writer_bench.cc)
  endif()
  if (${TINT_BUILD_HLSL_WRITER})
    list(APPEND TINT_BENCHMARK_SRCS lang/hlsl/writer/writer_bench.cc)
  endif()
  if (${TINT_BUILD_MSL_WRITER})
    list(APPEND TINT_BENCHMARK_SRCS lang/msl/writer/writer_bench.cc)
  endif()
  if (${TINT_BUILD_SPV_WRITER})
    list(APPEND TINT_BENCHMARK_SRCS lang/spirv/writer/writer_bench.cc)
  endif()
  if (${TINT_BUILD_WGSL_WRITER})
    list(APPEND TINT_BENCHMARK_SRCS lang/wgsl/writer/writer_bench.cc)
  endif()

  add_executable(tint-benchmark ${TINT_BENCHMARK_SRCS})
  set_target_properties(${target} PROPERTIES FOLDER "Benchmarks")

  tint_core_compile_options(tint-benchmark)

  target_link_libraries(tint-benchmark PRIVATE benchmark::benchmark libtint)

  if (TINT_EXTERNAL_BENCHMARK_CORPUS_DIR)
    # Glob all the files at TINT_EXTERNAL_BENCHMARK_CORPUS_DIR, and create a header
    # that lists these with the macros:
    # TINT_BENCHMARK_EXTERNAL_WGSL_PROGRAMS()
    # TINT_BENCHMARK_EXTERNAL_SPV_PROGRAMS()
    set(TINT_BENCHMARK_GEN_DIR "${DAWN_BUILD_GEN_DIR}/src/tint/benchmark/")
    set(TINT_BENCHMARK_EXTERNAL_SHADERS_HEADER "${TINT_BENCHMARK_GEN_DIR}/external_wgsl_programs.h")
    message("Globbing ${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}...")

    file(GLOB_RECURSE
      TINT_EXTERNAL_WGSL_BENCHMARK_FILES
      RELATIVE "${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}"
      "${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}/**.wgsl")
    list(TRANSFORM TINT_EXTERNAL_WGSL_BENCHMARK_FILES REPLACE
      "(.+)"
      "    BENCHMARK_CAPTURE\(FUNC, \"\\1\", \"${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}/\\1\")")
    list(JOIN TINT_EXTERNAL_WGSL_BENCHMARK_FILES "; \\\n" TINT_EXTERNAL_WGSL_BENCHMARK_FILES)

    file(GLOB_RECURSE
      TINT_EXTERNAL_SPV_BENCHMARK_FILES
      RELATIVE "${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}"
      "${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}/**.spv")

      list(TRANSFORM TINT_EXTERNAL_SPV_BENCHMARK_FILES REPLACE
      "(.+)"
      "    BENCHMARK_CAPTURE\(FUNC, \"\\1\", \"${TINT_EXTERNAL_BENCHMARK_CORPUS_DIR}/\\1\")")
    list(JOIN TINT_EXTERNAL_SPV_BENCHMARK_FILES "; \\\n" TINT_EXTERNAL_SPV_BENCHMARK_FILES)

    file(CONFIGURE
      OUTPUT "${TINT_BENCHMARK_EXTERNAL_SHADERS_HEADER}"
      CONTENT "
#define TINT_BENCHMARK_EXTERNAL_WGSL_PROGRAMS(FUNC) \\
${TINT_EXTERNAL_WGSL_BENCHMARK_FILES};

#define TINT_BENCHMARK_EXTERNAL_SPV_PROGRAMS(FUNC) \\
${TINT_EXTERNAL_SPV_BENCHMARK_FILES};")
    # Define TINT_BENCHMARK_EXTERNAL_SHADERS_HEADER to the generated header path
    target_compile_definitions(tint-benchmark PRIVATE
      "TINT_BENCHMARK_EXTERNAL_SHADERS_HEADER=\"${TINT_BENCHMARK_EXTERNAL_SHADERS_HEADER}\"")
  endif()
endif(TINT_BUILD_BENCHMARKS)
