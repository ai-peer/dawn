//* Copyright 2021 The Dawn Authors
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

#ifndef DAWNUTILS_DEBUGPRINT_H_
#define DAWNUTILS_DEBUGPRINT_H_

#include "dawn/webgpu_cpp.h"

namespace utils { namespace debug_print {

  {% for type in by_category["enum"] %}
      template <typename OStream>
      OStream& operator<<(OStream& o, wgpu::{{as_cppType(type.name)}} value) {
          switch (value) {
            {% for value in type.values %}
              case wgpu::{{as_cppType(type.name)}}::{{as_cppEnum(value.name)}}:
                o << "{{as_cppEnum(value.name)}}";
                break;
            {% endfor %}
          }
          return o;
      }
  {% endfor %}

  {% for type in by_category["bitmask"] %}
      template <typename OStream>
      OStream& operator<<(OStream& o, wgpu::{{as_cppType(type.name)}} value) {
        {% for value in type.values %}
          if (value & wgpu::{{as_cppType(type.name)}}::{{as_cppEnum(value.name)}}) {
            o << "{{as_cppEnum(value.name)}}";
          }
        {% endfor %}
        return o;
      }
  {% endfor %}

}}  // namespace utils::debug_print

#endif // DAWNUTILS_DEBUGPRINT_H_
