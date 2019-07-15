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

// A mock GLFW implementation that supporst Fuchsia, but only implements
// the functions called from Dawn.

// NOTE: This must be included before GLFW/glfw3.h because the latter will
// include <vulkan/vulkan.h> and "common/vulkan_platform.h" wants to be
// the first header to do so for sanity reasons (e.g. undefining weird
// macros on Windows and Linux).
#include "common/vulkan_platform.h"
#include <GLFW/glfw3.h>

#include <cassert>

int glfwInit(void) {
  return GLFW_TRUE;
}

void glfwDefaultWindowHints(void) {}

void glfwWindowHint(int hint, int value) {
  (void)hint;
  (void)value;
}

GLFWwindow* glfwCreateWindow(int width, int height, const char* title,
                             GLFWmonitor* monitor, GLFWwindow* share) {
  assert(monitor == nullptr);
  assert(share == nullptr);
  (void)width;
  (void)height;
  (void)title;
  return nullptr;
}

VkResult glfwCreateWindowSurface(VkInstance instance,
                                 GLFWwindow* window,
                                 const VkAllocationCallbacks* allocator,
                                 VkSurfaceKHR* surface) {
  *surface = VK_NULL_HANDLE;
  return VK_SUCCESS;
}
