// Copyright 2017 The Dawn Authors
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

#include "dawn_native/opengl/BufferGL.h"

#include "dawn_native/opengl/DeviceGL.h"

namespace dawn_native { namespace opengl {

    // Buffer

    Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
        : BufferBase(device, descriptor) {
        device->gl.GenBuffers(1, &mBuffer);
        device->gl.BindBuffer(GL_ARRAY_BUFFER, mBuffer);
        device->gl.BufferData(GL_ARRAY_BUFFER, GetSize(), nullptr, GL_STATIC_DRAW);
    }

    Buffer::~Buffer() {
        DestroyInternal();
    }

    GLuint Buffer::GetHandle() const {
        return mBuffer;
    }

    bool Buffer::IsMapWritable() const {
        // TODO(enga): All buffers in GL can be mapped. Investigate if mapping them will cause the
        // driver to migrate it to shared memory.
        return true;
    }

    MaybeError Buffer::MapAtCreationImpl(uint8_t** mappedPointer) {
        void* data = CallMapBuffer(GL_WRITE_ONLY);
        *mappedPointer = reinterpret_cast<uint8_t*>(data);
        return {};
    }

    MaybeError Buffer::SetSubDataImpl(uint32_t start, uint32_t count, const void* data) {
        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

        gl.BindBuffer(GL_ARRAY_BUFFER, mBuffer);
        gl.BufferSubData(GL_ARRAY_BUFFER, start, count, data);
        return {};
    }

    MaybeError Buffer::MapReadAsyncImpl(uint32_t serial) {
        void* data = CallMapBuffer(GL_READ_ONLY);
        CallMapReadCallback(serial, WGPUBufferMapAsyncStatus_Success, data, GetSize());
        return {};
    }

    MaybeError Buffer::MapWriteAsyncImpl(uint32_t serial) {
        void* data = CallMapBuffer(GL_WRITE_ONLY);
        CallMapWriteCallback(serial, WGPUBufferMapAsyncStatus_Success, data, GetSize());
        return {};
    }

    void Buffer::UnmapImpl() {
        if (GetSize() == 0) {
            // In WebGPU it is valid to map a 0-sized buffer but not in OpenGL, so skip it.
            return nullptr;
        }

        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

        gl.BindBuffer(GL_ARRAY_BUFFER, mBuffer);
        gl.UnmapBuffer(GL_ARRAY_BUFFER);
    }

    void Buffer::DestroyImpl() {
        ToBackend(GetDevice())->gl.DeleteBuffers(1, &mBuffer);
        mBuffer = 0;
    }

    void* Buffer::CallMapBuffer(GLenum flags) {
        if (GetSize() == 0) {
            // In WebGPU it is valid to map a 0-sized buffer but not in OpenGL, so skip it.
            return nullptr;
        }

        // TODO(cwallez@chromium.org): this does GPU->CPU synchronization, we could require a high
        // version of OpenGL that would let us map the buffer unsynchronized.
        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
        gl.BindBuffer(GL_ARRAY_BUFFER, mBuffer);
        void* data = gl.MapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    }

}}  // namespace dawn_native::opengl
