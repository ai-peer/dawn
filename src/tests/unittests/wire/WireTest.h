// Copyright 2019 The Dawn Authors
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

#include "gtest/gtest.h"
#include "mock/mock_dawn.h"

#include <memory>

// Definition of a "Lambda predicate matcher" for GMock to allow checking deep structures
// are passed correctly by the wire.

// Helper templates to extract the argument type of a lambda.
template <typename T>
struct MatcherMethodArgument;

template <typename Lambda, typename Arg>
struct MatcherMethodArgument<bool (Lambda::*)(Arg) const> {
    using Type = Arg;
};

template <typename Lambda>
using MatcherLambdaArgument = typename MatcherMethodArgument<decltype(&Lambda::operator())>::Type;

// The matcher itself, unfortunately it isn't able to return detailed information like other
// matchers do.
template <typename Lambda, typename Arg>
class LambdaMatcherImpl : public testing::MatcherInterface<Arg> {
  public:
    explicit LambdaMatcherImpl(Lambda lambda) : mLambda(lambda) {
    }

    void DescribeTo(std::ostream* os) const override {
        *os << "with a custom matcher";
    }

    bool MatchAndExplain(Arg value, testing::MatchResultListener* listener) const override {
        if (!mLambda(value)) {
            *listener << "which doesn't satisfy the custom predicate";
            return false;
        }
        return true;
    }

  private:
    Lambda mLambda;
};

// Use the MatchesLambda as follows:
//
//   EXPECT_CALL(foo, Bar(MatchesLambda([](ArgType arg) -> bool {
//       return CheckPredicateOnArg(arg);
//   })));
template <typename Lambda>
inline testing::Matcher<MatcherLambdaArgument<Lambda>> MatchesLambda(Lambda lambda) {
    return MakeMatcher(new LambdaMatcherImpl<Lambda, MatcherLambdaArgument<Lambda>>(lambda));
}

// Mock classes to add expectations on the wire calling callbacks
class MockDeviceErrorCallback {
  public:
    MOCK_METHOD2(Call, void(const char* message, dawnCallbackUserdata userdata));
};

class MockBuilderErrorCallback {
  public:
    MOCK_METHOD4(Call,
                 void(dawnBuilderErrorStatus status,
                      const char* message,
                      dawnCallbackUserdata userdata1,
                      dawnCallbackUserdata userdata2));
};

class MockBufferMapReadCallback {
  public:
    MOCK_METHOD4(Call,
                 void(dawnBufferMapAsyncStatus status,
                      const uint32_t* ptr,
                      uint32_t dataLength,
                      dawnCallbackUserdata userdata));
};

class MockBufferMapWriteCallback {
  public:
    MOCK_METHOD4(Call,
                 void(dawnBufferMapAsyncStatus status,
                      uint32_t* ptr,
                      uint32_t dataLength,
                      dawnCallbackUserdata userdata));
};

class MockFenceOnCompletionCallback {
  public:
    MOCK_METHOD2(Call, void(dawnFenceCompletionStatus status, dawnCallbackUserdata userdata));
};

namespace dawn_wire {
    class WireClient;
    class WireServer;
}  // namespace dawn_wire

namespace utils {
    class TerribleCommandBuffer;
}

class WireTest : public testing::Test {
  protected:
    WireTest(bool ignoreSetCallbackCalls);
    ~WireTest() override;

    void SetUp() override;
    void TearDown() override;
    void FlushClient();
    void FlushServer();

    static void ToMockDeviceErrorCallback(const char* message, dawnCallbackUserdata userdata);
    static void ToMockBuilderErrorCallback(dawnBuilderErrorStatus status,
                                           const char* message,
                                           dawnCallbackUserdata userdata1,
                                           dawnCallbackUserdata userdata2);
    static void ToMockBufferMapReadCallback(dawnBufferMapAsyncStatus status,
                                            const void* ptr,
                                            uint32_t dataLength,
                                            dawnCallbackUserdata userdata);
    static void ToMockBufferMapWriteCallback(dawnBufferMapAsyncStatus status,
                                             void* ptr,
                                             uint32_t dataLength,
                                             dawnCallbackUserdata userdata);
    static void ToMockFenceOnCompletionCallback(dawnFenceCompletionStatus status,
                                                dawnCallbackUserdata userdata);

    static std::unique_ptr<MockDeviceErrorCallback> mockDeviceErrorCallback;
    static std::unique_ptr<MockBuilderErrorCallback> mockBuilderErrorCallback;
    static std::unique_ptr<MockBufferMapReadCallback> mockBufferMapReadCallback;
    static std::unique_ptr<MockBufferMapWriteCallback> mockBufferMapWriteCallback;
    static uint32_t* lastMapWritePointer;
    static std::unique_ptr<MockFenceOnCompletionCallback> mockFenceOnCompletionCallback;

    MockProcTable api;
    dawnDevice apiDevice;
    dawnDevice device;

  private:
    bool mIgnoreSetCallbackCalls = false;

    std::unique_ptr<dawn_wire::WireServer> mWireServer;
    std::unique_ptr<dawn_wire::WireClient> mWireClient;
    std::unique_ptr<utils::TerribleCommandBuffer> mS2cBuf;
    std::unique_ptr<utils::TerribleCommandBuffer> mC2sBuf;
};
