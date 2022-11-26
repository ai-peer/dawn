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

#ifndef SRC_TINT_INTERP_UVEC3_H_
#define SRC_TINT_INTERP_UVEC3_H_

#include <array>
#include <cstdint>
#include <string>

#include "tint/utils/hash.h"

namespace tint::interp {

/// UVec3 is a wrapper around std::array<uint32_t, 3>.
class UVec3 {
  public:
    /// Default constructor - zero all values.
    UVec3() { values = {0, 0, 0}; }

    /// Constructor - initialize with values.
    /// @param x the x component
    /// @param y the y component
    /// @param z the z component
    UVec3(uint32_t x, uint32_t y, uint32_t z) { values = {x, y, z}; }

    /// Access an element (non-const).
    /// @param i the index to access
    /// @returns a reference to the element
    uint32_t& operator[](size_t i) { return values[i]; }

    /// Access an element (const).
    /// @param i the index to access
    /// @returns a const reference to the element
    const uint32_t& operator[](size_t i) const { return values[i]; }

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 is equal to `rhs`
    bool operator==(const UVec3& rhs) const { return values == rhs.values; }

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 is not equal to `rhs`
    bool operator!=(const UVec3& rhs) const { return values != rhs.values; }

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 has a lower value than `rhs`
    bool operator<(const UVec3& rhs) const {
        if (values[2] < rhs[2]) {
            return true;
        } else if (values[2] == rhs[2]) {
            if (values[1] < rhs[1]) {
                return true;
            } else if (values[1] == rhs[1]) {
                if (values[0] < rhs[0]) {
                    return true;
                }
            }
        }
        return false;
    }

    /// @returns a string representation of the values, as "(x, y, z)"
    std::string Str() const {
        return "(" + std::to_string(values[0]) + "," + std::to_string(values[1]) + "," +
               std::to_string(values[2]) + ")";
    }

  private:
    std::array<uint32_t, 3> values;
};

}  // namespace tint::interp

namespace std {

/// Custom std::hash specialization for UVec3 so that it can be used as a key for maps and sets.
template <>
class hash<tint::interp::UVec3> {
  public:
    /// @param vec the UVec3 to create a hash for
    /// @return the hash value
    inline std::size_t operator()(const tint::interp::UVec3& vec) const {
        return tint::utils::Hash(vec[0], vec[1], vec[2]);
    }
};

}  // namespace std

#endif  // SRC_TINT_INTERP_UVEC3_H_
