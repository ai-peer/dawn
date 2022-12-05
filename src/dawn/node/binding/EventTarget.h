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

#ifndef SRC_DAWN_NODE_BINDING_EVENTTARGET_H_
#define SRC_DAWN_NODE_BINDING_EVENTTARGET_H_

#include <string>

#include "src/dawn/node/interop/Napi.h"
#include "src/dawn/node/interop/WebGPU.h"

namespace wgpu::binding {

// Implements the machinery common to all EventTargets on the Web platform. There is a lot of
// complexity to support some of the legacy behaviors. For example setting device.onuncapturederror
// or passing a boolean for the options of AddEventListener.
class EventTarget : public interop::EventTarget {
  public:
    EventTarget();
    ~EventTarget();

    void AddEventListener(
        const std::string& type,
        const std::optional<interop::EventListener>& callback,
        const std::optional<std::variant<interop::AddEventListenerOptions, bool>>& options);
    void RemoveEventListener(
        const std::string& type,
        const std::optional<interop::EventListener>& callback,
        const std::optional<std::variant<interop::EventListenerOptions, bool>>& options);
    bool DispatchEvent(Napi::Env env, interop::Interface<interop::Event> event);

    interop::EventHandler GetEventHandlerAttribute(Napi::Env env, const std::string& type);
    void SetEventHandlerAttribute(Napi::Env env,
                                  const std::string& type,
                                  const interop::EventHandler& handler);

  private:
    struct Listener {
      public:
        Listener(const interop::EventListener& callback,
                 const interop::AddEventListenerOptions& options);
        Listener(const interop::EventListener& callback, bool capture);
        interop::EventListener callback;
        bool capture;
        bool once;
        bool passive;

        bool operator==(const Listener& other) const;
        bool operator!=(const Listener& other) const;
    };
    // To make all operations happen in O(1) we could have a hash_set<Listener> but it's not clear
    // how to create a hash that's stable in the face of a compacting garbage collector. So instead
    // we use a vector and have O(N) complexity and assume the vectors will be small.
    std::unordered_map<std::string, std::vector<Listener>> mListenersPerType;

    // Used to support the legacy `device.onuncaptureerror = callback`.
    std::unordered_map<std::string, interop::EventListenerCallback> mAttributeRegisteredListeners;
};

}  // namespace wgpu::binding

#endif  // SRC_DAWN_NODE_BINDING_EVENTTARGET_H_
