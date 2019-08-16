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

#include "dawn_native/opengl/IndirectUtilsGL.h"

#include <stdio.h>
#include "common/Assert.h"
#include "dawn_native/Commands.h"
#include "dawn_native/PassResourceUsage.h"
#include "dawn_native/opengl/BufferGL.h"
#include "dawn_native/opengl/Forward.h"

namespace dawn_native { namespace opengl {

    // TODO: remove temporary debug func
    void print_shader_info_log(GLuint shader) {
        int max_length = 4096;
        int actual_length = 0;
        char slog[4096];
        glGetShaderInfoLog(shader, max_length, &actual_length, slog);
        fprintf(stderr, "shader info log for GL index %u\n%s\n", shader, slog);
    }

    // TODO: remove temporary debug func
    bool check_shader_errors(GLuint shader) {
        GLint params = -1;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &params);
        if (GL_TRUE != params) {
            fprintf(stderr, "ERROR: shader %u did not compile\n", shader);
            print_shader_info_log(shader);
            return false;
        }
        return true;
    }

    const char* IndirectUtils::DRAW_INDEXED_SHADER_SRC = R"(
        #version 450
        struct DrawElementsIndirectCommand {
            uint count;
            uint primCount;
            uint firstIndex;
            uint baseVertex;
            uint baseInstance;
        };

        layout(std430, binding = 0) buffer inputBlock {
            uint inputBuf[];
        };

        layout(std430, binding = 1) buffer outputBlock {
            uint outputBuf[];
        };

        layout (location = 0) uniform int indexBufferOffset;
        layout (location = 1) uniform uint inLoc;
        layout (location = 2) uniform uint outLoc;

        layout (local_size_x = 1) in;
        void main() {
            for (uint i = 0; i < 5; i++) {
                outputBuf[outLoc + i] = inputBuf[inLoc + i];
            }
            outputBuf[outLoc + 2] += indexBufferOffset;
        }
    )";

    IndirectUtils::IndirectUtils() {
        glGenBuffers(1, &mBufferHandle);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mBufferHandle);
        // TODO: Make this buffer bigger
        glBufferData(GL_SHADER_STORAGE_BUFFER, 20 * sizeof(uint32_t), 0, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        CreatePrograms();
    }

    IndirectUtils::~IndirectUtils() {
        glDeleteBuffers(1, &mBufferHandle);
        mBufferHandle = 0;
        glDeleteProgram(mDrawIndexedProgram);
        mDrawIndexedProgram = 0;
    }

    void IndirectUtils::CreatePrograms() {
        mDrawIndexedProgram = CreateProgram(DRAW_INDEXED_SHADER_SRC);
    }

    GLuint IndirectUtils::CreateProgram(const char* shaderSrc) {
        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &shaderSrc, NULL);
        glCompileShader(computeShader);
        check_shader_errors(computeShader);

        GLuint program = glCreateProgram();
        glAttachShader(program, computeShader);
        glLinkProgram(program);
        glDetachShader(program, computeShader);
        glDeleteShader(computeShader);

        return program;
    }

    void IndirectUtils::ProcessDrawIndexed(uint64_t indirectBufferOffset,
                                           uint32_t indexBufferBaseOffset,
                                           uint32_t formatSize,
                                           GLuint indirectBufferHandle) const {
        glUseProgram(mDrawIndexedProgram);
        // TODO: limit indexBufferBaseOffset to multiple of formatSize
        glUniform1i(0, indexBufferBaseOffset / formatSize);
        glUniform1ui(1, indirectBufferOffset / sizeof(uint32_t));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, indirectBufferHandle);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mBufferHandle);
        glDispatchCompute(1, 1, 1);
    }

    void IndirectUtils::ProcessUsages(const std::vector<IndirectBufferUsage>& usages) const {
        uint64_t outOffset = 0;
        for (const IndirectBufferUsage& usage : usages) {
            // TODO: add other types
            glUseProgram(mDrawIndexedProgram);
            // TODO: limit indexBufferBaseOffset to multiple of formatSize
            // TODO: track format size?
            uint32_t formatSize = sizeof(uint32_t);
            glUniform1i(0, usage.indexBufferOffset / formatSize);
            glUniform1ui(1, usage.indirectOffset / sizeof(uint32_t));
            glUniform1ui(2, outOffset / sizeof(uint32_t));
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ToBackend(usage.buffer)->GetHandle());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mBufferHandle);
            glDispatchCompute(1, 1, 1);
            outOffset = NextBufferOffset(outOffset, Command::DrawIndexedIndirect);
        }
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    uint64_t IndirectUtils::NextBufferOffset(uint64_t currentOffset, Command type) const {
        switch (type) {
            case Command::DrawIndexedIndirect: {
                return currentOffset + DRAW_INDEXED_COMMAND_SIZE;
            } break;
            default: { UNREACHABLE(); } break;
        }
    }

}}  // namespace dawn_native::opengl
