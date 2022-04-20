// Copyright 2022 The Dawn Authors
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

#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"

#if defined(DAWN_ENABLE_BACKEND_OPENGL)
#    include "GLFW/glfw3.h"
#endif  // DAWN_ENABLE_BACKEND_OPENGL

int main(int argc, char** argv) {
    { DAWN_DEBUG() << "glfwInit"; }
    if (!glfwInit()) {
        {
            DAWN_DEBUG() << "glfwInit failed";
        }
        return 1;
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    { DAWN_DEBUG() << "glfwCreateWindow"; }
    GLFWwindow* window = glfwCreateWindow(400, 400, "Dawn OpenGLES test window2", nullptr, nullptr);
    if (!window) {
        {
            DAWN_DEBUG() << "glfwCreateWindow failed";
        }
        return 1;
    }

    { DAWN_DEBUG() << "glfwMakeContextCurrent"; }
    glfwMakeContextCurrent(window);

    return 0;
}
