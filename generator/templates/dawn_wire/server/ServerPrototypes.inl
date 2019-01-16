//* Copyright 2019 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

// Forwarding callbacks
static void ForwardDeviceErrorToServer(const char* message, dawnCallbackUserdata userdata);
static void ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status, const void* ptr, dawnCallbackUserdata userdata);
static void ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status, void* ptr, dawnCallbackUserdata userdata);
static void ForwardFenceCompletedValue(dawnFenceCompletionStatus status, dawnCallbackUserdata userdata);
{% for type in by_category["object"] if type.is_builder%}
    static void Forward{{type.name.CamelCase()}}ToClient(dawnBuilderErrorStatus status, const char* message, dawnCallbackUserdata userdata1, dawnCallbackUserdata userdata2);
{% endfor %}

// Error callbacks
void OnDeviceError(const char* message);
{% for type in by_category["object"] if type.is_builder%}
    {% set Type = type.name.CamelCase() %}
    void On{{Type}}Error(dawnBuilderErrorStatus status, const char* message, uint32_t id, uint32_t serial);
{% endfor %}

void OnMapReadAsyncCallback(dawnBufferMapAsyncStatus status, const void* ptr, MapUserdata* userdata);
void OnMapWriteAsyncCallback(dawnBufferMapAsyncStatus status, void* ptr, MapUserdata* userdata);
void OnFenceCompletedValueUpdated(FenceCompletionUserdata* userdata);

// Command handlers
bool PreHandleBufferUnmap(const BufferUnmapCmd& cmd);
bool PostHandleQueueSignal(const QueueSignalCmd& cmd);
bool HandleBufferMapAsync(const char** commands, size_t* size);
bool HandleBufferUpdateMappedData(const char** commands, size_t* size);
bool HandleDestroyObject(const char** commands, size_t* size);
{% for type in by_category["object"] %}
    {% for method in type.methods %}
        {% set Suffix = as_MethodSuffix(type.name, method.name) %}
        {% if Suffix not in client_side_commands %}
            bool Handle{{Suffix}}(const char** commands, size_t* size);
        {% endif %}
    {% endfor %}
{% endfor %}
