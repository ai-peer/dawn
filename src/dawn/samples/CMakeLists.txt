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

add_subdirectory(utils)

if (DAWN_BUILD_ANDROID_SAMPLES)
  add_subdirectory(android/chellotriangle)
  add_subdirectory(android/cpphellotriangle)

else()

add_executable(CppHelloTriangle "CppHelloTriangle.cpp")
common_compile_options(CppHelloTriangle)
target_link_libraries(CppHelloTriangle dawn_sample_utils)

add_executable(CHelloTriangle "CHelloTriangle.cpp")
common_compile_options(CHelloTriangle)
target_link_libraries(CHelloTriangle dawn_sample_utils)

add_executable(ComputeBoids "ComputeBoids.cpp")
common_compile_options(ComputeBoids)
target_link_libraries(ComputeBoids dawn_sample_utils)

add_executable(Animometer "Animometer.cpp")
common_compile_options(Animometer)
target_link_libraries(Animometer dawn_sample_utils)

add_executable(DawnInfo "DawnInfo.cpp")
common_compile_options(DawnInfo)
target_link_libraries(DawnInfo dawn_sample_utils)

endif()

