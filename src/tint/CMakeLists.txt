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

################################################################################
# Target flags
################################################################################
if (NOT TINT_BUILD_AS_OTHER_OS)
  if(UNIX)
    set(IS_LINUX TRUE)
    set(IS_LINUX TRUE)
  elseif(APPLE)
    set(IS_MAC TRUE)
    set(IS_MAC TRUE)
  elseif(WIN32)
    set(IS_WINDOWS TRUE)
    set(IS_WINDOWS TRUE)
  endif()
endif()

################################################################################
# Helper functions
################################################################################
function(tint_core_compile_options TARGET)
  target_include_directories(${TARGET} PUBLIC "${TINT_ROOT_SOURCE_DIR}")
  target_include_directories(${TARGET} PUBLIC "${TINT_ROOT_SOURCE_DIR}/include")

  if (${TINT_BUILD_SPV_READER} OR ${TINT_BUILD_SPV_WRITER})
    target_include_directories(${TARGET} PUBLIC
        "${DAWN_THIRD_PARTY_DIR}/spirv-headers/include")
  endif()

  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_SPV_READER=$<BOOL:${TINT_BUILD_SPV_READER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_WGSL_READER=$<BOOL:${TINT_BUILD_WGSL_READER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_GLSL_WRITER=$<BOOL:${TINT_BUILD_GLSL_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_HLSL_WRITER=$<BOOL:${TINT_BUILD_HLSL_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_MSL_WRITER=$<BOOL:${TINT_BUILD_MSL_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_SPV_WRITER=$<BOOL:${TINT_BUILD_SPV_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_WGSL_WRITER=$<BOOL:${TINT_BUILD_WGSL_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_SYNTAX_TREE_WRITER=$<BOOL:${TINT_BUILD_SYNTAX_TREE_WRITER}>)
  target_compile_definitions(${TARGET} PUBLIC -DTINT_BUILD_IR=$<BOOL:${TINT_BUILD_IR}>)

  common_compile_options(${TARGET})
endfunction()

function(tint_default_compile_options TARGET)
  tint_core_compile_options(${TARGET})

  set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX)

  set(COMMON_GNU_OPTIONS
    -Wall
    -Werror
    -Wextra
    -Wno-documentation-unknown-command
    -Wno-padded
    -Wno-switch-enum
    -Wno-unknown-pragmas
  )

  set(COMMON_CLANG_OPTIONS
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-format-pedantic
    -Wno-poison-system-directories
    -Wno-return-std-move-in-c++11
    -Wno-unknown-warning-option
    -Wno-undefined-var-template
    -Wno-unsafe-buffer-usage
    -Wno-used-but-marked-unused
    -Weverything
  )

  if (COMPILER_IS_LIKE_GNU)
    target_compile_options(${TARGET} PRIVATE
      -pedantic-errors
      ${COMMON_GNU_OPTIONS}
    )

    if (COMPILER_IS_CLANG)
      target_compile_options(${TARGET} PRIVATE
        ${COMMON_CLANG_OPTIONS}
      )
    endif()
  endif(COMPILER_IS_LIKE_GNU)

  if (MSVC)
    # Specify /EHs for exception handling.
    target_compile_options(${TARGET} PRIVATE
      /bigobj
      /EHsc
      /W4
      /WX
      /wd4068 # unknown pragma
      /wd4127 # conditional expression is constant
      /wd4244 # 'conversion' conversion from 'type1' to 'type2', possible loss of data
      /wd4267 # 'var' : conversion from 'size_t' to 'type', possible loss of data
      /wd4324 # 'struct_name' : structure was padded due to __declspec(align())
      /wd4459 # declaration of 'identifier' hides global declaration
      /wd4458 # declaration of 'identifier' hides class member
      /wd4514 # 'function' : unreferenced inline function has been removed
      /wd4571 # catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
      /wd4625 # 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
      /wd4626 # 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
      /wd4710 # 'function' : function not inlined
      /wd4774 # 'function' : format string 'string' requires an argument of type 'type', but variadic argument number has type 'type'
      /wd4820 # 'bytes' bytes padding added after construct 'member_name'
      /wd5026 # 'type': move constructor was implicitly defined as deleted
      /wd5027 # 'type': move assignment operator was implicitly defined as deleted
    )

    # When building with clang-cl on Windows, try to match our clang build
    # options as much as possible.
    if (COMPILER_IS_CLANG_CL)
      target_compile_options(${TARGET} PRIVATE
        ${COMMON_GNU_OPTIONS}
        ${COMMON_CLANG_OPTIONS}
        # Disable warnings that are usually disabled in downstream deps for
        # gcc/clang, but aren't for clang-cl.
        -Wno-global-constructors
        -Wno-zero-as-null-pointer-constant
        -Wno-shorten-64-to-32
        -Wno-shadow-field-in-constructor
        -Wno-reserved-id-macro
        -Wno-language-extension-token
      )
    endif()
  endif()

  if (TINT_RANDOMIZE_HASHES)
    if(NOT DEFINED TINT_HASH_SEED)
        string(RANDOM LENGTH 16 ALPHABET "0123456789abcdef" seed)
        set(TINT_HASH_SEED "0x${seed}" CACHE STRING "Tint hash seed value")
        message("Using TINT_HASH_SEED: ${TINT_HASH_SEED}")
    endif()
    target_compile_definitions(${TARGET} PUBLIC "-DTINT_HASH_SEED=${TINT_HASH_SEED}")
  endif()
endfunction()

function(tint_test_compile_options TARGET)
  tint_default_compile_options(${TARGET})

  set_target_properties(${TARGET} PROPERTIES FOLDER "Tests")

  target_include_directories(${TARGET} PRIVATE
    ${gmock_SOURCE_DIR}/include
  )

  target_link_libraries(${TARGET}
    gmock
  )

  if(MSVC)
    # TODO(crbug.com/tint/1749): MSVC debug builds can suffer from stack
    # overflows when resolving deeply nested expression chains or statements.
    # Production builds neither use MSVC nor debug, so just bump the stack size
    # for this build combination.
    if (IS_DEBUG_BUILD)
      target_link_options(${TARGET} PRIVATE "/STACK:4194304") # 4MB, default is 1MB
    endif()
  else()
    target_compile_options(${TARGET} PRIVATE
      -Wno-global-constructors
      -Wno-weak-vtables
    )
  endif()
endfunction()

function(tint_bench_compile_options TARGET)
  tint_default_compile_options(${TARGET})

  set_target_properties(${TARGET} PROPERTIES FOLDER "Benchmarks")

  target_link_libraries(${TARGET} PRIVATE benchmark::benchmark)
endfunction()

function(tint_cmd_compile_options TARGET)
  tint_default_compile_options(${TARGET})
  target_link_libraries(${TARGET} libtint)
endfunction()

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
  lib_tint_lang_core_constant
  lib_tint_lang_core_type
  lib_tint_lang_wgsl_ast_transform
  lib_tint_lang_wgsl_inspector
  lib_tint_lang_wgsl_sem
  lib_tint_utils_strconv
  lib_tint_utils_text
)

if(TINT_BUILD_SPV_READER)
  target_link_libraries(libtint
    lib_tint_lang_spirv_reader
  )
endif()

if(TINT_BUILD_SPV_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_spirv_writer
  )
endif()

if(TINT_BUILD_WGSL_READER)
  target_link_libraries(libtint
    lib_tint_lang_wgsl_reader
  )
endif()

if(TINT_BUILD_WGSL_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_wgsl_writer
  )
endif()

if(TINT_BUILD_MSL_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_msl_writer
  )
endif()

if(TINT_BUILD_HLSL_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_hlsl_writer
  )
endif()

if(TINT_BUILD_GLSL_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_glsl_writer
  )
endif()

if(TINT_BUILD_SYNTAX_TREE_WRITER)
  target_link_libraries(libtint
    lib_tint_lang_wgsl_writer_syntax_tree_printer
  )
endif()

if(TINT_BUILD_IR)
  target_link_libraries(libtint
    lib_tint_lang_core_ir
    lib_tint_lang_wgsl_writer_ir_to_program
  )
endif()

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
  add_executable(tint_unittests
    "main_test.cc"
  )

  if (${TINT_BUILD_FUZZERS})
    target_sources(tint_unittests PRIVATE
      fuzzers/mersenne_twister_engine.cc
      fuzzers/mersenne_twister_engine.h
      fuzzers/random_generator.cc
      fuzzers/random_generator.h
      fuzzers/random_generator_engine.cc
      fuzzers/random_generator_engine.h
      fuzzers/random_generator_test.cc
    )
  endif()

  tint_test_compile_options(tint_unittests)

  target_link_libraries(tint_unittests libtint)

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
    "main_bench.cc"
    "utils/rtti/switch_bench.cc"
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

################################################################################
# Include the generated build files
################################################################################
include("BUILD.cmake")

################################################################################
# Fudges. Needs to be moved into BUILD.cmake files
################################################################################
target_link_libraries(lib_tint_utils_strconv absl_strings)

if(TINT_BUILD_GLSL_WRITER)
  target_link_libraries(tint_cmd_tint_cmd glslang)
  target_link_libraries(tint_cmd_tint_cmd glslang-default-resource-limits)
  if (NOT MSVC)
    target_compile_options(tint_cmd_tint_cmd PRIVATE
      -Wno-reserved-id-macro
      -Wno-shadow-field-in-constructor
      -Wno-shadow
      -Wno-weak-vtables
    )
  endif()
endif()

if(TINT_BUILD_SPV_WRITER OR TINT_BUILD_SPV_READER)
  tint_spvtools_compile_options(lib_tint_lang_spirv_reader_ast_parser)
  tint_spvtools_compile_options(lib_tint_lang_spirv_writer_common)
  tint_spvtools_compile_options(tint_cmd_tint_cmd)
  tint_spvtools_compile_options(lib_tint_cmd_common)
  tint_spvtools_compile_options(lib_tint_lang_core_ir_transform)
  tint_spvtools_compile_options(tint_unittests)
endif()
