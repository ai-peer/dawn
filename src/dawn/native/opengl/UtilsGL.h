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

#ifndef DAWNNATIVE_OPENGL_UTILSGL_H_
#define DAWNNATIVE_OPENGL_UTILSGL_H_

#include "dawn/native/Format.h"
#include "dawn/native/dawn_platform.h"
#include "dawn/native/opengl/opengl_platform.h"

namespace dawn::native::opengl {
    struct OpenGLFunctions;

    GLuint ToOpenGLCompareFunction(wgpu::CompareFunction compareFunction);
    GLint GetStencilMaskFromStencilFormat(wgpu::TextureFormat depthStencilFormat);
    void CopyImageSubData(const OpenGLFunctions& gl,
                          Aspect srcAspects,
                          GLuint srcHandle,
                          GLenum srcTarget,
                          GLint srcLevel,
                          GLint srcX,
                          GLint srcY,
                          GLint srcZ,
                          GLuint dstHandle,
                          GLenum dstTarget,
                          GLint dstLevel,
                          GLint dstX,
                          GLint dstY,
                          GLint dstZ,
                          GLsizei srcWidth,
                          GLsizei srcHeight,
                          GLsizei srcDepth);

}  // namespace dawn::native::opengl

#endif  // DAWNNATIVE_OPENGL_UTILSGL_H_
