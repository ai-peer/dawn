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

#include "SampleUtils.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

#include <napi.h>
#include <cassert>  // TODO: replace asserts with JS exceptions
#include <vector>

class Buffer : public Napi::ObjectWrap<Buffer> {
  public:
    static Napi::FunctionReference constructor;

    static void Init(const Napi::Env& env, Napi::Object* exports) {
        Napi::HandleScope scope(env);

        Napi::Function func = DefineClass(env, "Buffer",
                                          {
                                              InstanceMethod("setSubData", &Buffer::setSubData),
                                              InstanceMethod("mapReadAsync", &Buffer::mapReadAsync),
                                          });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("Buffer", func);
    }

  public:
    explicit Buffer(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Buffer>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::Buffer buffer) {
        mBuffer = buffer;
    }

    void setSubData(const Napi::CallbackInfo& info) {
        assert(info.Length() == 2);
        uint32_t start = info[0].As<Napi::Number>().Uint32Value();
        Napi::ArrayBuffer data = info[1].As<Napi::ArrayBuffer>();
        mBuffer.SetSubData(start, data.ByteLength(), static_cast<const uint8_t*>(data.Data()));
    }

    static void MapReadCallback(dawnBufferMapAsyncStatus status,
                                const void* data,
                                dawnCallbackUserdata userdata_) {
    }

    struct Userdata {
        Napi::Env env;
        Napi::FunctionReference callback;
        size_t size;
        Userdata(Napi::Env env, Napi::FunctionReference callback, size_t size)
            : env(env), callback(std::move(callback)), size(size) {
        }
    };
    static void cb(dawnBufferMapAsyncStatus status,
                   const void* data,
                   dawnCallbackUserdata userdata) {
        const Userdata* u = reinterpret_cast<const Userdata*>(userdata);

        assert(status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS);
        Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(u->env, u->size);
        memcpy(ab.Data(), data, u->size);
        u->callback.Call(u->env.Global(), {ab});
        delete u;
    };

    void mapReadAsync(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        assert(info.Length() == 3);
        uint32_t start = info[0].As<Napi::Number>().Uint32Value();
        size_t size = info[1].As<Napi::Number>().Uint32Value();
        Napi::Function callback = info[2].As<Napi::Function>();

        Userdata* userdata = new Userdata(env, Napi::Persistent(callback), size);
        mBuffer.MapReadAsync(
            start, size, cb,
            static_cast<dawn::CallbackUserdata>(reinterpret_cast<uintptr_t>(userdata)));
    }

    const dawn::Buffer& buffer() const {
        return mBuffer;
    }

  private:
    dawn::Buffer mBuffer;
};
Napi::FunctionReference Buffer::constructor;

class CommandBuffer : public Napi::ObjectWrap<CommandBuffer> {
  public:
    static Napi::FunctionReference constructor;

    static void Init(const Napi::Env& env, Napi::Object* exports) {
        Napi::HandleScope scope(env);

        Napi::Function func = DefineClass(
            env, "CommandBuffer",
            {
                InstanceMethod("copyBufferToBuffer", &CommandBuffer::copyBufferToBuffer),
            });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("CommandBuffer", func);
    }

  public:
    explicit CommandBuffer(const Napi::CallbackInfo& info) : Napi::ObjectWrap<CommandBuffer>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::CommandBufferBuilder builder) {
        mCommandBufferBuilder = builder;
    }

    dawn::CommandBuffer takeCommandBuffer() {
        assert(mCommandBufferBuilder);
        dawn::CommandBufferBuilder builder;
        std::swap(mCommandBufferBuilder, builder);
        return builder.GetResult();
    }

    void copyBufferToBuffer(const Napi::CallbackInfo& info) {
        assert(info.Length() == 5);
        mCommandBufferBuilder.CopyBufferToBuffer(
            Buffer::Unwrap(info[0].As<Napi::Object>())->buffer(),
            info[1].As<Napi::Number>().Uint32Value(),
            Buffer::Unwrap(info[2].As<Napi::Object>())->buffer(),
            info[3].As<Napi::Number>().Uint32Value(), info[4].As<Napi::Number>().Uint32Value());
    }

  private:
    dawn::CommandBufferBuilder mCommandBufferBuilder;
};
Napi::FunctionReference CommandBuffer::constructor;

class Queue : public Napi::ObjectWrap<Queue> {
  public:
    static Napi::FunctionReference constructor;

    static void Init(const Napi::Env& env, Napi::Object* exports) {
        Napi::HandleScope scope(env);

        Napi::Function func = DefineClass(env, "Queue",
                                          {
                                              InstanceMethod("submit", &Queue::submit),
                                          });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("Queue", func);
    }

  public:
    explicit Queue(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Queue>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::Device device) {
        mQueue = device.CreateQueue();
    }

  private:
    dawn::Queue mQueue;

    void submit(const Napi::CallbackInfo& info) {
        assert(info.Length() == 1);
        Napi::Array buffers = info[0].As<Napi::Array>();

        std::vector<dawn::CommandBuffer> bufs(buffers.Length());
        for (size_t i = 0; i < buffers.Length(); ++i) {
            Napi::Value buf = buffers[i];
            bufs[i] = CommandBuffer::Unwrap(buf.As<Napi::Object>())->takeCommandBuffer();
        }
        mQueue.Submit(bufs.size(), bufs.data());
    }
};
Napi::FunctionReference Queue::constructor;

class Device : public Napi::ObjectWrap<Device> {
  public:
    static Napi::FunctionReference constructor;

    static void Init(const Napi::Env& env, Napi::Object* exports) {
        Napi::HandleScope scope(env);

        Napi::Function func =
            DefineClass(env, "Device",
                        {
                            InstanceMethod("getQueue", &Device::getQueue),
                            InstanceMethod("createBuffer", &Device::createBuffer),
                            InstanceMethod("createCommandBuffer", &Device::createCommandBuffer),
                            InstanceMethod("flush", &Device::flush),
                        });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("Device", func);
    }

  public:
    explicit Device(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Device>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::Device device) {
        mDevice = device;
        mQueue = Queue::constructor.New({});
        Queue::Unwrap(mQueue)->init(mDevice);
    }

  private:
    dawn::Device mDevice;
    Napi::Object mQueue;

    Napi::Value getQueue(const Napi::CallbackInfo& info) {
        assert(info.Length() == 0);
        return mQueue;
    }

    Napi::Value createBuffer(const Napi::CallbackInfo& info) {
        assert(info.Length() == 1);

        Napi::Object desc = info[0].As<Napi::Object>();
        dawn::BufferDescriptor descriptor{};
        descriptor.usage = static_cast<dawn::BufferUsageBit>(
            static_cast<Napi::Value>(desc["usage"]).As<Napi::Number>().Uint32Value());
        descriptor.size = static_cast<Napi::Value>(desc["size"]).As<Napi::Number>().Uint32Value();

        Napi::Object cmd = Buffer::constructor.New({});
        Buffer::Unwrap(cmd)->init(mDevice.CreateBuffer(&descriptor));
        return cmd;
    }

    Napi::Value createCommandBuffer(const Napi::CallbackInfo& info) {
        assert(info.Length() == 1);
        /* Napi::Object desc = */ info[0].As<Napi::Object>();
        Napi::Object cmd = CommandBuffer::constructor.New({});
        CommandBuffer::Unwrap(cmd)->init(mDevice.CreateCommandBufferBuilder());
        return cmd;
    }

    void flush(const Napi::CallbackInfo& info) {
        mDevice.Tick();
        DoFlush();  // TODO: flush more intelligently?
    }
};
Napi::FunctionReference Device::constructor;

namespace {

    Napi::Value getDevice(const Napi::CallbackInfo& info) {
        assert(info.Length() == 0);
        Napi::Object device = Device::constructor.New({});
        Device::Unwrap(device)->init(CreateCppDawnDevice());
        return device;
    }

    Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
        Device::Init(env, &exports);
        Queue::Init(env, &exports);
        Buffer::Init(env, &exports);
        CommandBuffer::Init(env, &exports);

        exports.Set(Napi::String::New(env, "getDevice"), Napi::Function::New(env, getDevice));
        return exports;
    }

    NODE_API_MODULE(hello, InitAll)

}  // namespace
