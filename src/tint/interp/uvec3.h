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

#include "tint/utils/math/hash.h"

namespace tint::interp {

/// UVec3 is a wrapper around std::array<uint32_t, 3>.
class UVec3 {
  public:
    /// Default constructor - zero all values.
    UVec3() {}

    /// Constructor - initialize with values.
    /// @param x_value the x component
    /// @param y_value the y component
    /// @param z_value the z component
    UVec3(uint32_t x_value, uint32_t y_value, uint32_t z_value)
        : x(x_value), y(y_value), z(z_value) {}

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 is equal to `rhs`
    bool operator==(const UVec3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 is not equal to `rhs`
    bool operator!=(const UVec3& rhs) const { return !(*this == rhs); }

    /// @param rhs the other UVec3 to compare to
    /// @returns true if this UVec3 has a lower value than `rhs`
    bool operator<(const UVec3& rhs) const {
        if (z < rhs.z) {
            return true;
        } else if (z == rhs.z) {
            if (y < rhs.y) {
                return true;
            } else if (y == rhs.y) {
                if (x < rhs.x) {
                    return true;
                }
            }
        }
        return false;
    }

    /// @returns a string representation of the values, as "(x, y, z)"
    std::string Str() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }

    /// The X component.
    uint32_t x = 0;
    /// The Y component.
    uint32_t y = 0;
    /// The Z component.
    uint32_t z = 0;
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
        return tint::Hash(vec.x, vec.y, vec.z);
    }
};

}  // namespace std

#endif  // SRC_TINT_INTERP_UVEC3_H_
