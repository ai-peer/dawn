// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/type/rw_byte_address_buffer.h"

#include <gtest/gtest.h>

#include "src/tint/lang/core/type/f32.h"

namespace tint::hlsl::type {
namespace {

TEST(HlslTypeRwByteAddressBuffer, Equals) {
    core::type::F32 f{};
    core::type::Pointer ptr1(core::AddressSpace::kStorage, &f, core::Access::kReadWrite);
    core::type::Pointer ptr2(core::AddressSpace::kUniform, &f, core::Access::kReadWrite);

    const RwByteAddressBuffer a(&ptr1);
    const RwByteAddressBuffer b(&ptr1);
    const RwByteAddressBuffer c(&ptr2);
    const core::type::F32 d{};

    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.Equals(c));
    EXPECT_FALSE(a.Equals(d));
}

TEST(HlslTypeRwByteAddressBuffer, FriendlyName) {
    core::type::F32 f{};
    core::type::Pointer ptr1(core::AddressSpace::kStorage, &f, core::Access::kReadWrite);
    const RwByteAddressBuffer l(&ptr1);

    EXPECT_EQ(l.FriendlyName(), "hlsl.rw_byte_address_buffer<storage, f32, read_write>");
}

}  // namespace
}  // namespace tint::hlsl::type
