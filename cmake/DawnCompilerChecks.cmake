# Copyright 2024 The Dawn & Tint Authors
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# TODO: Figure out the minimum GCC
# # Minimum compiler version check: GCC >= 9.0
# if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
#     CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
#   message(FATAL_ERROR "GCC 9.0 or later is required.")
# endif ()

# TODO: Figure out the minimum clang
# # Minimum compiler version check: LLVM Clang >= 14.0
# if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
#     CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0)
#   message(FATAL_ERROR "LLVM Clang 14.0 or later is required.")
# endif ()

# TODO: Figure out the minimum xcode
# # Minimum compiler version check: Apple Clang >= 15.0 Xcode 15.0
# if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND
#     CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
#   message(FATAL_ERROR "Apple Clang 15.0 or later is required.")
# endif ()

# TODO: Figure out the minimum msvc
# # Minimum compiler version check: Microsoft C/C++ >= 19.20 (aka VS 2019)
# if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
#     CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.20)
#   message(FATAL_ERROR "Microsoft Visual Studio 2019 or later is required.")
# endif ()

# Make sure we have C++17 enabled.
# Needed to make sure libraries and executables not built by the
# dawn_add_library still have the C++17 compiler flags enabled
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)
