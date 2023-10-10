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

#ifndef SRC_TINT_UTILS_BYTES_READER_H_
#define SRC_TINT_UTILS_BYTES_READER_H_

#include <cstdint>

#include "src/tint/utils/bytes/endianness.h"
#include "src/tint/utils/bytes/swap.h"
#include "src/tint/utils/containers/slice.h"

namespace tint::bytes {

/// A binary stream reader.
struct Reader {
    /// @returns true if there are no more bytes remaining
    bool IsEOF() const { return offset >= bytes.len; }

    /// @returns the number of bytes remaining in the stream
    size_t BytesRemaining() const { return IsEOF() ? 0 : bytes.len - offset; }

    /// Reads an integer from the stream, performing byte swapping if the stream's endianness
    /// differs from the native endianness. If there are too few bytes remaining in the stream, then
    /// the missing data will be substituted with zeros.
    /// @return the deserialized integer
    template <typename T>
    T Int() {
        static_assert(std::is_integral_v<T>);
        T out = 0;
        if (!IsEOF()) {
            size_t n = std::min(sizeof(T), BytesRemaining());
            memcpy(&out, &bytes[offset], n);
            offset += n;
            if (NativeEndianness() != endianness) {
                out = Swap(out);
            }
        }
        return out;
    }

    /// Reads a float from the stream. If there are too few bytes remaining in the stream, then
    /// the missing data will be substituted with zeros.
    /// @return the deserialized floating point number
    template <typename T>
    T Float() {
        static_assert(std::is_floating_point_v<T>);
        T out = 0;
        if (!IsEOF()) {
            size_t n = std::min(sizeof(T), BytesRemaining());
            memcpy(&out, &bytes[offset], n);
            offset += n;
        }
        return out;
    }

    /// Reads a boolean from the stream
    /// @returns true if the next byte is non-zero
    bool Bool() {
        if (IsEOF()) {
            return false;
        }
        bool out = bytes[offset] != 0;
        offset++;
        return out;
    }

    /// Reads a string of @p len bytes from the stream. If there are too few bytes remaining in the
    /// stream, then the returned string will be truncated.
    /// @return the deserialized string
    std::string String(size_t len) {
        if (IsEOF()) {
            return "";
        }
        size_t n = std::min(len, BytesRemaining());
        std::string out(reinterpret_cast<const char*>(&bytes[offset]), n);
        offset += n;
        return out;
    }

    /// The data to read from
    Slice<const uint8_t> bytes;

    /// The current byte offset
    size_t offset = 0;

    /// The endianness of integers serialized in the stream
    Endianness endianness = Endianness::kLittle;
};

/// Reads the templated type from the reader and assigns it to @p out
/// @note This function does not
template <typename T>
Reader& operator>>(Reader& reader, T& out) {
    constexpr bool is_numeric = std::is_integral_v<T> || std::is_floating_point_v<T>;
    static_assert(is_numeric);

    if constexpr (std::is_integral_v<T>) {
        out = reader.Int<T>();
        return reader;
    }

    if constexpr (std::is_floating_point_v<T>) {
        out = reader.Float<T>();
        return reader;
    }

    // Unreachable
    return reader;
}

}  // namespace tint::bytes

#endif  // SRC_TINT_UTILS_BYTES_READER_H_
