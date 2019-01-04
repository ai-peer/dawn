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
#include <cassert>  // TODO replace asserts with JS exceptions
#include <vector>

class CommandBuffer : public Napi::ObjectWrap<CommandBuffer> {
  public:
    static Napi::FunctionReference constructor;

    static void Init(const Napi::Env& env, Napi::Object* exports) {
        Napi::HandleScope scope(env);

        Napi::Function func = DefineClass(env, "CommandBuffer", {});

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("CommandBuffer", func);
    }

  public:
    explicit CommandBuffer(const Napi::CallbackInfo& info) : Napi::ObjectWrap<CommandBuffer>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::CommandBufferBuilder&& builder) {
        mCommandBufferBuilder = std::move(builder);
    }

    const dawn::CommandBuffer takeCommandBuffer() {
        assert(mCommandBufferBuilder);
        dawn::CommandBufferBuilder builder;
        std::swap(mCommandBufferBuilder, builder);
        return builder.GetResult();
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

    void init(dawn::Queue&& queue) {
        mQueue = std::move(queue);
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
        DoFlush();  // TODO: flush more intelligently?
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
                            InstanceMethod("createCommandBuffer", &Device::createCommandBuffer),
                        });

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        exports->Set("Device", func);
    }

  public:
    explicit Device(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Device>(info) {
        assert(info.Length() == 0);
    }

    void init(dawn::Device&& device) {
        mDevice = std::move(device);
        mQueue = Queue::constructor.New({});
        Queue::Unwrap(mQueue)->init(mDevice.CreateQueue());
    }

  private:
    dawn::Device mDevice;
    Napi::Object mQueue;

    Napi::Value getQueue(const Napi::CallbackInfo& info) {
        assert(info.Length() == 0);
        return mQueue;
    }

    Napi::Value createCommandBuffer(const Napi::CallbackInfo& info) {
        assert(info.Length() == 0);
        Napi::Object cmd = CommandBuffer::constructor.New({});
        CommandBuffer::Unwrap(cmd)->init(mDevice.CreateCommandBufferBuilder());
        return cmd;
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
        CommandBuffer::Init(env, &exports);

        exports.Set(Napi::String::New(env, "getDevice"), Napi::Function::New(env, getDevice));
        return exports;
    }

    NODE_API_MODULE(hello, InitAll)

}  // namespace
