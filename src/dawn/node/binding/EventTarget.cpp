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

#include "src/dawn/node/binding/EventTarget.h"

namespace wgpu::binding {

EventTarget::EventTarget() = default;
EventTarget::~EventTarget() = default;

EventTarget::Listener::Listener(const interop::EventListener& callback,
                                const interop::AddEventListenerOptions& options)
    : callback(callback), capture(options.capture), once(options.once), passive(options.passive) {}

EventTarget::Listener::Listener(const interop::EventListener& callback, bool capture)
    : callback(callback), capture(capture), once(false), passive(false) {}

bool EventTarget::Listener::operator==(const Listener& other) const {
    // Other members are not compared on purpose because two event listener differing only in once /
    // passive can't be added to the same EventTarget.
    return callback == other.callback && capture == other.capture;
}
bool EventTarget::Listener::operator!=(const Listener& other) const {
    return !(*this == other);
}

void EventTarget::AddEventListener(
    const std::string& type,
    const std::optional<interop::EventListener>& callbackIn,
    const std::optional<std::variant<interop::AddEventListenerOptions, bool>>& optionsIn) {
    // A null callback means that we should exit early (it is allowed in the Web platform for some
    // kind of feature detection but does nothing).
    if (!callbackIn.has_value()) {
        return;
    }
    interop::EventListener callback = callbackIn.value();

    // Reify the options to the "modern" Web platform options.
    interop::AddEventListenerOptions options;
    if (optionsIn.has_value()) {
        if (const bool* capture = std::get_if<bool>(&optionsIn.value())) {
            options.capture = *capture;
        } else {
            options = std::get<interop::AddEventListenerOptions>(optionsIn.value());
        }
    }

    Listener listenerToAdd(callback, options);

    // The same listener cannot be added twice.
    std::vector<Listener>& listeners = mListenersPerType[type];
    for (const Listener& listener : listeners) {
        if (listener == listenerToAdd) {
            return;
        }
    }

    listeners.push_back(listenerToAdd);
}

void EventTarget::RemoveEventListener(
    const std::string& type,
    const std::optional<interop::EventListener>& callbackIn,
    const std::optional<std::variant<interop::EventListenerOptions, bool>>& optionsIn) {
    // A null callback means that we should exit early (it is allowed in the Web platform for some
    // kind of feature detection but does nothing).
    if (!callbackIn.has_value()) {
        return;
    }
    interop::EventListener callback = callbackIn.value();

    // Early out if there is no listener for this event type.
    if (mListenersPerType.count(type) == 0) {
        return;
    }
    std::vector<Listener>& listeners = mListenersPerType[type];
    assert(!listeners.empty());  // Necessary for the erase-remove idiom.

    // Reify the options to a single capture boolean.
    bool capture = false;
    if (optionsIn.has_value()) {
        if (const bool* captureIn = std::get_if<bool>(&optionsIn.value())) {
            capture = *captureIn;
        } else {
            capture = std::get<interop::EventListenerOptions>(optionsIn.value()).capture;
        }
    }

    Listener listenerToAdd(callback, capture);
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                   [&](const Listener& other) { return other == listenerToAdd; }),
                    listeners.end());
    if (listeners.empty()) {
        mListenersPerType.erase(type);
    }
}

bool EventTarget::DispatchEvent(Napi::Env env, interop::Interface<interop::Event> event) {
    // The event dispatch rules are incredibly complicated and involve a lot of DOM-isms. For WebGPU
    // we don't need to implement all of them and can simplify the code a bit. Features implemented
    // are:
    //
    //   - The listener's `once` value is honored.
    //   - Listeners can be either a function or an object with `handleEvent`.
    std::string type = event->getType(env);

    // Early out if there is no listener for this event type.
    if (mListenersPerType.count(type) == 0) {
        return true;
    }
    std::vector<Listener>& listeners = mListenersPerType[type];
    assert(!listeners.empty());  // Necessary for the erase-remove idiom.

    for (const Listener& listener : listeners) {
        if (const auto* callback =
                std::get_if<interop::EventListenerCallback>(&listener.callback)) {
            callback->Call({event});
        } else {
            Napi::Object obj =
                std::get<interop::Interface<interop::EventListenerInterface>>(listener.callback);

            // TODO: does the dispatchEvent spec require any special behavior when failing to call
            // handleEvent?
            Napi::Value attribute = obj.Get("handleEvent");
            if (!attribute.IsEmpty() || !attribute.IsFunction()) {
                continue;
            }

            Napi::Function handleEvent = attribute.As<Napi::Function>();
            handleEvent.Call(obj, {event});
        }
    }

    // Remove all `once` listeners since they were invoked.
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                   [&](const Listener& listener) { return listener.once; }),
                    listeners.end());
    if (listeners.empty()) {
        mListenersPerType.erase(type);
    }

    // We don't support cancellable events so the event is never cancelled.
    return true;
}

interop::EventHandler EventTarget::GetEventHandlerAttribute(Napi::Env env,
                                                            const std::string& type) {
    auto it = mAttributeRegisteredListeners.find(type);
    if (it == mAttributeRegisteredListeners.end()) {
        return {};
    }
    return {it->second};
}

void EventTarget::SetEventHandlerAttribute(Napi::Env env,
                                           const std::string& type,
                                           const interop::EventHandler& handler) {
    // Remove the previous listener if any.
    auto it = mAttributeRegisteredListeners.find(type);
    if (it != mAttributeRegisteredListeners.end()) {
        RemoveEventListener(type, {{it->second}}, {});
    }

    if (!handler.has_value()) {
        mAttributeRegisteredListeners.erase(type);
        return;
    }

    mAttributeRegisteredListeners[type] = handler.value();
    AddEventListener(type, {{handler.value()}}, {});
}

}  // namespace wgpu::binding
