// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
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
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_
#define SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_

#include <cstdint>

#include "src/tint/utils/containers/enum_set.h"

namespace tint {

class TextStyle {
  public:
    using Bits = uint16_t;

    static constexpr Bits kStyleMask /*          */ = 0b0000'0000'0000'1111;
    static constexpr Bits kStyleCode /*          */ = 0b0000'0000'0000'1000;
    static constexpr Bits kStyleBold /*          */ = 0b0000'0000'0000'0100;
    static constexpr Bits kStyleItalic /*        */ = 0b0000'0000'0000'0010;
    static constexpr Bits kStyleUnderlined /*    */ = 0b0000'0000'0000'0001;

    static constexpr Bits kSeverityMask /*       */ = 0b0000'0000'1111'0000;
    static constexpr Bits kSeverityDefault /*    */ = 0b0000'0000'0000'0000;
    static constexpr Bits kSeveritySuccess /*    */ = 0b0000'0000'0001'0000;
    static constexpr Bits kSeverityWarning /*    */ = 0b0000'0000'0010'0000;
    static constexpr Bits kSeverityError /*      */ = 0b0000'0000'0011'0000;
    static constexpr Bits kSeverityFatal /*      */ = 0b0000'0000'0100'0000;

    static constexpr Bits kKindMask /*           */ = 0b0000'1111'0000'0000;
    static constexpr Bits kKindVariable /*       */ = 0b0000'0001'0000'0000;
    static constexpr Bits kKindType /*           */ = 0b0000'0010'0000'0000;
    static constexpr Bits kKindFunction /*       */ = 0b0000'0011'0000'0000;
    static constexpr Bits kKindEnum /*           */ = 0b0000'0100'0000'0000;
    static constexpr Bits kKindOperator /*       */ = 0b0000'0101'0000'0000;
    static constexpr Bits kKindSquiggle /*       */ = 0b0000'0110'0000'0000;

    bool IsCode() const { return (bits & kStyleCode) != 0; }
    bool IsBold() const { return (bits & kStyleBold) != 0; }
    bool IsItalic() const { return (bits & kStyleItalic) != 0; }
    bool IsUnderlined() const { return (bits & kStyleUnderlined) != 0; }

    bool IsSuccess() const { return (bits & kSeverityMask) == kSeveritySuccess; }
    bool IsWarning() const { return (bits & kSeverityMask) == kSeverityWarning; }
    bool IsError() const { return (bits & kSeverityMask) == kSeverityError; }
    bool IsFatal() const { return (bits & kSeverityMask) == kSeverityFatal; }

    bool IsVariable() const { return (bits & kKindMask) == kKindVariable; }
    bool IsType() const { return (bits & kKindMask) == kKindType; }
    bool IsFunction() const { return (bits & kKindMask) == kKindFunction; }
    bool IsEnum() const { return (bits & kKindMask) == kKindEnum; }
    bool IsOperator() const { return (bits & kKindMask) == kKindOperator; }
    bool IsSquiggle() const { return (bits & kKindMask) == kKindSquiggle; }

    TextStyle& Code(bool value = true) {
        SetBit(kStyleCode, value);
        return *this;
    }

    TextStyle& Bold(bool value = true) {
        SetBit(kStyleBold, value);
        return *this;
    }

    TextStyle& Italic(bool value = true) {
        SetBit(kStyleItalic, value);
        return *this;
    }

    TextStyle& Underlined(bool value = true) {
        SetBit(kStyleUnderlined, value);
        return *this;
    }

    TextStyle& Success() {
        SetEnum(kSeveritySuccess, kSeverityMask);
        return *this;
    }

    TextStyle& Warning() {
        SetEnum(kSeverityWarning, kSeverityMask);
        return *this;
    }

    TextStyle& Error() {
        SetEnum(kSeverityError, kSeverityMask);
        return *this;
    }

    TextStyle& Fatal() {
        SetEnum(kSeverityFatal, kSeverityMask);
        return *this;
    }

    TextStyle& Variable() {
        SetEnum(kKindVariable, kKindMask);
        return *this;
    }

    TextStyle& Type() {
        SetEnum(kKindType, kKindMask);
        return *this;
    }

    TextStyle& Function() {
        SetEnum(kKindFunction, kKindMask);
        return *this;
    }

    TextStyle& Enum() {
        SetEnum(kKindEnum, kKindMask);
        return *this;
    }

    TextStyle& Operator() {
        SetEnum(kKindOperator, kKindMask);
        return *this;
    }

    TextStyle& Squiggle() {
        SetEnum(kKindSquiggle, kKindMask);
        return *this;
    }

    /// Equality operator
    bool operator==(const TextStyle& other) const { return bits == other.bits; }

    /// Inequality operator
    bool operator!=(const TextStyle& other) const { return bits != other.bits; }

    Bits bits = 0;

  private:
    void SetBit(Bits bit, bool enable) {
        if (enable) {
            bits |= bit;
        } else {
            bits = bits & ~bit;
        }
    }

    void SetEnum(Bits value, Bits mask) { bits = (bits & ~mask) | value; }
};

}  // namespace tint

#endif  // SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_
