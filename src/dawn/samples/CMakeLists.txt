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

function (dawn_add_sample name extra_sources extra_object_targets extra_depends)

  dawn_add_executable(
    ${name}
    UTILITY_TARGET dawn::dawnbuild
    SOURCES
      "${name}.cpp"
      "SampleUtils.cpp"
      "SampleUtils.h"
      ${extra_sources}
    OBJECT_TARGETS
      ${extra_object_targets}
    DEPENDS
      dawn::dawn_glfw
      dawn::dawn_native
      dawn::dawn_proc
      dawn::dawn_utils
      dawn::dawncpp_headers
      glfw
      ${extra_depends}
  )
endfunction ()

set(extra_sources)
set(extra_object_targets)
set(extra_depends)
if (TARGET SPIRV-Tools-opt)
  list(APPEND extra_object_targets
    SPIRV-Tools-opt)
endif ()

dawn_add_sample(HelloTriangle "${extra_sources}" "${extra_object_targets}" "${extra_depends}")
dawn_add_sample(ComputeBoids "${extra_sources}" "${extra_object_targets}" "${extra_depends}")
dawn_add_sample(Animometer "${extra_sources}" "${extra_object_targets}" "${extra_depends}")
dawn_add_sample(DawnInfo "${extra_sources}" "${extra_object_targets}" "${extra_depends}")
dawn_add_sample(ManualSurfaceTest "${extra_sources}" "${extra_object_targets}" "${extra_depends}")
