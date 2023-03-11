# Copyright 2023 The Tint Authors.
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

set(TINT_INTERP_SRCS
  data_race_detector.cc
  data_race_detector.h
  interactive_debugger.cc
  interactive_debugger.h
  invocation.cc
  invocation.h
  memory.cc
  memory.h
  shader_executor.cc
  shader_executor.h
  uvec3.h
  workgroup.cc
  workgroup.h
)

## libtint-interp library
add_library(tint-interp ${TINT_INTERP_SRCS})
target_include_directories(tint-interp PUBLIC "${DAWN_SRC_DIR}")
target_link_libraries(tint-interp libtint)

if(${TINT_BUILD_SPV_READER} OR ${TINT_BUILD_SPV_WRITER})
  target_link_libraries(tint-interp SPIRV-Tools)
endif()

# Check for GNU readline library.
# This is used to improve the command-line debugger interface.
find_library(READLINE_LIBRARY readline)
find_path(READLINE_INCLUDES readline/readline.h)
if (READLINE_LIBRARY AND READLINE_INCLUDES)
  target_link_libraries(tint-interp ${READLINE_LIBRARY})
  target_include_directories(tint-interp PRIVATE ${READLINE_INCLUDES})
  target_compile_definitions(tint-interp PRIVATE TINT_DEBUGGER_ENABLE_READLINE)
else()
  message(STATUS "Tint interpreter: GNU readline library not found")
endif()

if(${TINT_BUILD_TESTS})
  ## tint-interp-tests executable
  add_executable(tint-interp-tests
                 tests/test_main.cc
                 tests/data_race_detector_tests.cc
                 tests/end2end_tests.cc
                 tests/interactive_debugger_tests.cc
                 tests/invocation_tests.cc
                 tests/memory_tests.cc
                 tests/shader_executor_tests.cc)
  target_include_directories(tint-interp-tests PUBLIC "${DAWN_SRC_DIR}")
  target_link_libraries(tint-interp-tests tint-interp gmock)

  if(NOT MSVC)
    target_compile_options(tint-interp-tests PRIVATE
      -Wno-ctad-maybe-unsupported
      -Wno-global-constructors
      -Wno-weak-vtables
    )
  endif()
endif()
