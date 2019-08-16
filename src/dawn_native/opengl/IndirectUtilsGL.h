// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DAWNNATIVE_OPENGL_INDIRECTUTILSGL_H_
#define DAWNNATIVE_OPENGL_INDIRECTUTILSGL_H_

#include "dawn_native/Commands.h"
#include "dawn_native/PassResourceUsage.h"
#include "dawn_native/dawn_platform.h"
#include "glad/glad.h"

namespace dawn_native { namespace opengl {

    class IndirectUtils {
      public:
        IndirectUtils();
        ~IndirectUtils();

        void ProcessDrawIndexed(uint64_t indirectBufferOffset,
                                uint32_t indexBufferBaseOffset,
                                uint32_t formatSize,
                                GLuint indirectBufferHandle) const;
        void ProcessUsages(const std::vector<IndirectBufferUsage>& usages) const;

        uint64_t NextBufferOffset(uint64_t currentOffset, Command type) const;

        GLuint mBufferHandle;

      private:
        static const char* DRAW_INDEXED_SHADER_SRC;
        static const uint64_t DRAW_INDEXED_COMMAND_SIZE = 5 * sizeof(uint32_t);

        GLuint mDrawIndexedProgram;

        void CreatePrograms();
        GLuint CreateProgram(const char* shaderSrc);
    };

}}  // namespace dawn_native::opengl

#endif  // DAWNNATIVE_OPENGL_INDIRECTUTILSGL_H_
