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

add_library(dawn_common STATIC)
target_sources(dawn_common
    PUBLIC
        "Assert.h"
        "BitSetIterator.h"
        "Compiler.h"
        "Constants.h"
        "DynamicLib.h"
        "GPUInfo.h"
        "HashUtils.h"
        "Log.h"
        "Math.h"
        "Platform.h"
        "Result.h"
        "Serial.h"
        "SerialMap.h"
        "SerialQueue.h"
        "SerialStorage.h"
        "SwapChainUtils.h"
        "SystemUtils.h"
        "vulkan_platform.h"
        "windows_with_undefs.h"
        "xlib_with_undefs.h"
    PRIVATE
        "Assert.cpp"
        "DynamicLib.cpp"
        "GPUInfo.cpp"
        "Log.cpp"
        "Math.cpp"
        "Result.cpp"
        "SystemUtils.cpp"
)
target_link_libraries(dawn_common PRIVATE dawn_internal_config)

# TODO Android Log support
# TODO Vulkan headers support
