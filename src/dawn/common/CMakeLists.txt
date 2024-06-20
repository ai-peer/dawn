# Copyright 2020 The Dawn & Tint Authors
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

DawnGenerator(
  SCRIPT "${Dawn_SOURCE_DIR}/generator/dawn_version_generator.py"
  PRINT_NAME "Dawn version based utilities"
  ARGS "--dawn-dir"
      "${Dawn_SOURCE_DIR}"
  RESULT_VARIABLE "DAWN_VERSION_AUTOGEN_SOURCES"
)

DawnGenerator(
  SCRIPT "${Dawn_SOURCE_DIR}/generator/dawn_gpu_info_generator.py"
  PRINT_NAME "Dawn GPU info utilities"
  ARGS "--gpu-info-json"
      "${Dawn_SOURCE_DIR}/src/dawn/gpu_info.json"
  RESULT_VARIABLE "DAWN_GPU_INFO_AUTOGEN_SOURCES"
)

set(headers
  "${DAWN_VERSION_AUTOGEN_SOURCES}"
  "${DAWN_GPU_INFO_AUTOGEN_SOURCES}"
)

set(sources
  "AlignedAlloc.cpp"
  "AlignedAlloc.h"
  "Alloc.h"
  "Assert.cpp"
  "Assert.h"
  "BitSetIterator.h"
  "Compiler.h"
  "Constants.h"
  "ContentLessObjectCache.h"
  "ContentLessObjectCacheable.h"
  "CoreFoundationRef.h"
  "DynamicLib.cpp"
  "DynamicLib.h"
  "egl_platform.h"
  "Enumerator.h"
  "FutureUtils.cpp"
  "FutureUtils.h"
  "GPUInfo.cpp"
  "GPUInfo.h"
  "HashUtils.h"
  "IOKitRef.h"
  "ityp_array.h"
  "ityp_bitset.h"
  "ityp_span.h"
  "ityp_stack_vec.h"
  "ityp_vector.h"
  "LinkedList.h"
  "Log.cpp"
  "Log.h"
  "MatchVariant.h"
  "Math.cpp"
  "Math.h"
  "Mutex.cpp"
  "Mutex.h"
  "MutexProtected.h"
  "NonCopyable.h"
  "NonMovable.h"
  "NSRef.h"
  "Numeric.h"
  "PlacementAllocated.h"
  "Platform.h"
  "Preprocessor.h"
  "Range.h"
  "Ref.h"
  "RefBase.h"
  "RefCounted.cpp"
  "RefCounted.h"
  "Result.cpp"
  "Result.h"
  "SerialMap.h"
  "SerialQueue.h"
  "SerialStorage.h"
  "SlabAllocator.cpp"
  "SlabAllocator.h"
  "StackAllocated.h"
  "SystemUtils.cpp"
  "SystemUtils.h"
  "TypedInteger.h"
  "TypeTraits.h"
  "UnderlyingType.h"
  "vulkan_platform.h"
  "WeakRef.h"
  "WeakRefSupport.cpp"
  "WeakRefSupport.h"
  "WGSLFeatureMapping.h"
  "xlib_with_undefs.h"
)

if (WIN32)
  list(APPEND sources
    "windows_with_undefs.h"
    "WindowsUtils.cpp"
    "WindowsUtils.h"
  )
elseif(APPLE)
  list(APPEND sources
    "IOSurfaceUtils.cpp"
    "IOSurfaceUtils.h"
    "SystemUtils_mac.mm"
  )
endif()

set(conditional_public_depends)
set(conditional_private_depends)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  find_library(log_lib log)
  list(APPEND conditional_private_depends
      ${log_lib}
      android)
endif ()

if (DAWN_ENABLE_VULKAN)
  list(APPEND conditional_private_depends
      Vulkan-Headers)
endif ()

dawn_add_library(
  dawn_common
  UTILITY_TARGET dawn_internal_config
  HEADERS
    ${headers}
  SOURCES
    ${sources}
  DEPENDS
    dawn::dawn_headers
    dawn::dawncpp_headers
    ${conditional_public_depends}
  PRIVATE_DEPENDS
    absl::inlined_vector
    dawn::partition_alloc
    ${conditional_private_depends}
)
