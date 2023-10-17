// Copyright 2023 The Tint Authors.
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

#ifndef SRC_TINT_LANG_GLSL_WRITER_COMMON_PRINTER_SUPPORT_H_
#define SRC_TINT_LANG_GLSL_WRITER_COMMON_PRINTER_SUPPORT_H_

#include <cstdint>

#include "src/tint/utils/text/string_stream.h"

namespace tint::glsl::writer {

/// Prints a float32 to the output stream
/// @param out the stream to write too
/// @param value the float32 value
void PrintF32(StringStream& out, float value);

/// Prints a float16 to the output stream
/// @param out the stream to write too
/// @param value the float16 value
void PrintF16(StringStream& out, float value);

/// Prints an int32 to the output stream
/// @param out the stream to write too
/// @param value the int32 value
void PrintI32(StringStream& out, int32_t value);

}  // namespace tint::glsl::writer

#endif  // SRC_TINT_LANG_GLSL_WRITER_COMMON_PRINTER_SUPPORT_H_
