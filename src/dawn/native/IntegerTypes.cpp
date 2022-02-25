// Copyright 2020 The Dawn Authors
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

#include "dawn/native/IntegerTypes.h"

namespace dawn::native {

    absl::FormatConvertResult<absl::FormatConversionCharSet::kString> AbslFormatConvert(
        const PipelineCompatibilityToken& value,
        const absl::FormatConversionSpec& spec,
        absl::FormatSink* s) {
        static const auto* const fmt = new absl::ParsedFormat<'u'>("%u");
        s->Append(absl::StrFormat(*fmt, static_cast<uint64_t>(value)));
        return {true};
    }

}  // namespace dawn::native
