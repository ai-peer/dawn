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

#include <algorithm>
#include <array>
#include <sstream>
#include <iomanip>

#include "dawn/common/GPUInfo_autogen.h"

#include "dawn/common/Assert.h"

namespace gpu_info {

// Vendor checks
{% for vendor in vendors %}
    bool Is{{vendor.name.CamelCase()}}(PCIVendorID vendorId) {
        return vendorId == kVendorID_{{vendor.name.CamelCase()}};
    }
{% endfor %}

// Architecture checks

{% for vendor in vendors %}
    {% if len(vendor.architectures) %}
        // {{vendor.name.get()}} architectures
        {% for architecture in vendor.architectures %}
            bool Is{{architecture.name.CamelCase()}}(PCIVendorID vendorId, PCIDeviceID deviceId) {
                if (vendorId != kVendorID_{{vendor.name.CamelCase()}}) {
                    return false;
                }

                switch (deviceId{{vendor.maskDeviceId()}}) {
                    {% for device in architecture.devices %}
                        case {{device}}:
                    {% endfor %}
                        return true;
                }

                return false;
            }
        {% endfor %}
    {% endif %}
{% endfor %}

// GPUAdapterInfo fields
std::string GetVendorName(PCIVendorID vendorId) {
    switch(vendorId) {
        {% for vendor in vendors %}
            case kVendorID_{{vendor.name.CamelCase()}}: return "{{vendor.name.js_enum_case()}}";
        {% endfor %}
    }

    return "";
}

std::string GetArchitectureName(PCIVendorID vendorId, PCIDeviceID deviceId) {
    switch(vendorId) {
        {% for vendor in vendors %}
            {% if len(vendor.architectures) %}

                case kVendorID_{{vendor.name.CamelCase()}}: {
                    switch (deviceId{{vendor.maskDeviceId()}}) {
                        {% for architecture in vendor.architectures %}
                            {% for device in architecture.devices %}
                                case {{device}}:
                            {% endfor %}
                                return "{{architecture.name.js_enum_case()}}";
                        {% endfor %}
                    }
                } break;
            {% endif %}
        {% endfor %}
    }

    return "";
}

std::string GetDeviceName(PCIVendorID vendorId, PCIDeviceID deviceId) {
    switch(vendorId) {
        // Must be a known vendor, or the deviceId may be ambiguous
        {% for vendor in vendors %}
            case kVendorID_{{vendor.name.CamelCase()}}:
        {% endfor %}
        {
            std::ostringstream out;
            out << "0x" << std::setw(4) << std::setfill('0') << std::hex << deviceId;
            return out.str();
        }
    }
    return "";
}

}  // namespace gpu_info
