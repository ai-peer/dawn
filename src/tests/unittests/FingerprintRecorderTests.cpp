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

#include <gtest/gtest.h>

#include "dawn_native/CachedObject.h"
#include "dawn_native/FingerprintRecorder.h"

using namespace dawn_native;

class Object : public CachedObject {
  public:
    Object(size_t value) : CachedObject(/*device*/ nullptr), mValue(value) {
    }
    void Fingerprint(FingerprintRecorder* recorder) {
        recorder->Record(mValue);
    }
    size_t mValue = 0;
};

class ObjectWithChild : public CachedObject {
  public:
    ObjectWithChild(Object* child) : CachedObject(/*device*/ nullptr), mChild(child) {
    }
    void Fingerprint(FingerprintRecorder* recorder) {
        recorder->Record(mChild);
    }
    Object* mChild = nullptr;
};

// Test recording the same object twice always produces the same hash.
TEST(FingerprintRecorderTests, RecordTwice) {
    constexpr size_t kDummyValue = 1234;

    Object obj(kDummyValue);
    FingerprintRecorder recorder;
    recorder.Record(&obj);

    const size_t hash = obj.GetHashForTesting();
    recorder.Record(&obj);
    EXPECT_EQ(obj.GetHashForTesting(), hash);
}

// Test recording two objects of same content produces the same hash when using separate
// recorders.
TEST(FingerprintRecorderTests, DiffRecorder) {
    constexpr size_t kDummyValue = 1234;

    {
        Object a(kDummyValue);
        FingerprintRecorder recorderA;
        recorderA.Record(&a);

        Object b(kDummyValue);
        FingerprintRecorder recorderB;
        recorderB.Record(&b);

        EXPECT_EQ(a.GetHashForTesting(), b.GetHashForTesting());
    }

    {
        Object c1(kDummyValue);
        ObjectWithChild p1(&c1);
        FingerprintRecorder recorderA;
        recorderA.Record(&p1);

        Object c2(kDummyValue);
        ObjectWithChild p2(&c2);
        FingerprintRecorder recorderB;
        recorderB.Record(&p2);

        // Parent and child objects used different recorders, they must hash the same.
        EXPECT_EQ(c1.GetHashForTesting(), c2.GetHashForTesting());
        EXPECT_EQ(p1.GetHashForTesting(), p2.GetHashForTesting());
    }
}

// Test recording two objects of same content produces different hash when using the same
// recorder.
TEST(FingerprintRecorderTests, SameRecorder) {
    constexpr size_t kDummyValue = 1234;

    {
        FingerprintRecorder recorder;

        Object a(kDummyValue);
        recorder.Record(&a);

        Object b(kDummyValue);
        recorder.Record(&b);

        EXPECT_NE(a.GetHashForTesting(), b.GetHashForTesting());
    }

    {
        FingerprintRecorder recorder;

        Object c1(kDummyValue);
        ObjectWithChild p1(&c1);
        recorder.Record(&p1);

        Object c2(kDummyValue);
        ObjectWithChild p2(&c2);
        recorder.Record(&p2);

        // Parent and child objects used same recorder, they cannot hash the same.
        EXPECT_NE(c1.GetHashForTesting(), c2.GetHashForTesting());
        EXPECT_NE(p1.GetHashForTesting(), p2.GetHashForTesting());
    }
}