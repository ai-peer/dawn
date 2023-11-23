// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <array>
#include <span>
#include <vector>

#include "dawn/common/Compiler.h"
#include "dawn/common/Enumerator.h"
#include "dawn/common/ityp_array.h"
#include "dawn/common/ityp_span.h"
#include "dawn/common/ityp_vector.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

class EnumeratorTest : public testing::Test {
  protected:
    template <typename Index, typename Thing, typename Value>
    void Check(Thing thingToEnumerate, std::vector<Index> indices, const Value* values) {
        size_t i = 0;
        for (auto [index, value] : Enumerate(thingToEnumerate)) {
            ASSERT_EQ(index, indices[i]);
            ASSERT_EQ(value, values[i]);
            i++;
        }
    }

    template <typename Thing>
    void CheckEmpty(Thing thingToEnumerate) {
        for (auto [index, value] : Enumerate(thingToEnumerate)) {
            DAWN_UNUSED(index);
            DAWN_UNUSED(value);
            FAIL();
        }
    }

    using Int = TypedInteger<struct IntT, size_t>;
};

// Test that Enumerate works with std::array
TEST_F(EnumeratorTest, StdArray) {
    // Empty array
    std::array<uint32_t, 0> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty array
    std::array<uint32_t, 3> thing = {37, 45, 67};
    Check<size_t>(thing, {{0, 1, 2}}, thing.data());
}

// Test that Enumerate works with std::span
TEST_F(EnumeratorTest, StdSpan) {
    // Empty span
    std::span<uint32_t> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty span
    std::array<uint32_t, 3> backingArray = {{37, 45, 67}};
    std::span<uint32_t> thing{backingArray.data(), 3};
    Check<size_t>(thing, {{0, 1, 2}}, thing.data());
}

// Test that Enumerate works with std::vector
TEST_F(EnumeratorTest, StdVector) {
    // Empty vector
    std::vector<uint32_t> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty vector
    std::vector<uint32_t> thing = {{37, 45, 67}};
    Check<size_t>(thing, {{0, 1, 2}}, thing.data());
}

// Test that Enumerate works with ityp::array
TEST_F(EnumeratorTest, ITypArray) {
    // Empty array
    ityp::array<Int, uint32_t, 0> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty array
    ityp::array<Int, uint32_t, 3> thing = {37u, 45u, 67u};
    Check<Int>(thing, {{Int(0), Int(1), Int(2)}}, thing.data());
}

// Test that Enumerate works with ityp::span
TEST_F(EnumeratorTest, ITypSpan) {
    // Empty span
    ityp::span<Int, uint32_t> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty span
    std::array<uint32_t, 3> backingArray = {{37, 45, 67}};
    auto thing = ityp::SpanFromUntyped<Int>(backingArray.data(), 3);
    Check<Int>(thing, {{Int(0), Int(1), Int(2)}}, thing.data());
}

// Test that Enumerate works with ityp::vector
TEST_F(EnumeratorTest, ITypVector) {
    // Empty vector
    ityp::vector<Int, uint32_t> emptyThing;
    CheckEmpty(emptyThing);

    // Non-empty vector
    ityp::vector<Int, uint32_t> thing = {{37, 45, 67}};
    Check<Int>(thing, {{Int(0), Int(1), Int(2)}}, thing.data());
}
}  // anonymous namespace
}  // namespace dawn
