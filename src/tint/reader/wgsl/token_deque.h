// Copyright 2022 The Tint Authors.
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

#ifndef SRC_TINT_READER_WGSL_TOKEN_DEQUE_H_
#define SRC_TINT_READER_WGSL_TOKEN_DEQUE_H_

#include <vector>

#include "src/tint/reader/wgsl/token.h"

namespace tint::reader::wgsl {

/// A deque for Token parsing. This uses a ring buffer internally and is sized
/// to hold enough tokens for a re-sync to happen, but not many more.
class TokenDeque {
  public:
    /// The number of tokens to hold in the deque
    static constexpr size_t kBufferSize = 40;

    TokenDeque();
    TokenDeque(const TokenDeque&) = delete;
    TokenDeque(TokenDeque&&) = delete;
    ~TokenDeque();

    TokenDeque& operator=(const TokenDeque&) = delete;
    TokenDeque& operator=(TokenDeque&&) = delete;

    /// @returns true if the deque is empty
    inline bool empty() const { return size() == 0; }

    /// @returns the number of elements in the deque
    inline size_t size() const {
        if (back_ < front_) {
            return kBufferSize - front_ + back_;
        }
        return back_ - front_;
    }

    /// @returns the token at the front of the deque
    inline Token pop_front() {
        auto old_front = front_++;
        if (front_ == kBufferSize) {
            front_ = 0;
        }
        return tokens_[old_front];
    }

    /// Pushes a token onto the front of the deque
    /// @param t the token to push
    inline void push_front(Token t) {
        // Wrap to end if needed.
        if (front_ == 0) {
            front_ = kBufferSize - 1;
        } else {
            front_--;
        }
        tokens_[front_] = t;
    }

    /// Pushes a token onto the back of the deque
    /// @param t the token to push
    inline void push_back(Token t) {
        tokens_[back_++] = t;
        // Wrap to begining of queue
        if (back_ == kBufferSize) {
            back_ = 0;
        }
    }

    /// @param idx the index to retrieve from
    /// @returns the token at the given index.
    inline const Token& operator[](size_t idx) const {
        if (front_ + idx >= kBufferSize) {
            return tokens_[idx - (kBufferSize - front_)];
        }
        return tokens_[front_ + idx];
    }

  private:
    std::vector<Token> tokens_;
    // The position of the first element
    size_t front_ = 0;
    // The position of one past the last element
    size_t back_ = 0;
};

}  // namespace tint::reader::wgsl

#endif  // SRC_TINT_READER_WGSL_TOKEN_DEQUE_H_
