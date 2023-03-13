// Copyright 2023 The Dawn Authors
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

namespace DawnLPMFuzzer {

static constexpr int instance_object_id = 1;
static constexpr int invalid_object_id = 0;

{% for type in by_category["object"] %}
    {% if type.name.canonical_case() in cmd_records["lpm_info"]["limits"] %}
        static constexpr int {{ type.name.snake_case() }}_limit = {{ cmd_records["lpm_info"]["limits"][type.name.canonical_case()] }};
    {% else %}
        static constexpr int {{ type.name.snake_case() }}_limit = {{ cmd_records["lpm_info"]["limits"]["default"] }};
    {% endif %}
{% endfor %}

} // namespace DawnLPMFuzzer
