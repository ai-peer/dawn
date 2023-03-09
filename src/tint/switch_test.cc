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

#include "src/tint/switch.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"

namespace tint {
namespace {

struct Animal : public tint::Castable<Animal> {};
struct Amphibian : public tint::Castable<Amphibian, Animal> {};
struct Mammal : public tint::Castable<Mammal, Animal> {};
struct Reptile : public tint::Castable<Reptile, Animal> {};
struct Frog : public tint::Castable<Frog, Amphibian> {};
struct Bear : public tint::Castable<Bear, Mammal> {};
struct Lizard : public tint::Castable<Lizard, Reptile> {};
struct Gecko : public tint::Castable<Gecko, Lizard> {};
struct Iguana : public tint::Castable<Iguana, Lizard> {};

TEST(Castable, SwitchNoDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        bool frog_matched_amphibian = false;
        Switch(
            frog.get(),  //
            [&](Reptile*) { FAIL() << "frog is not reptile"; },
            [&](Mammal*) { FAIL() << "frog is not mammal"; },
            [&](Amphibian* amphibian) {
                EXPECT_EQ(amphibian, frog.get());
                frog_matched_amphibian = true;
            });
        EXPECT_TRUE(frog_matched_amphibian);
    }
    {
        bool bear_matched_mammal = false;
        Switch(
            bear.get(),  //
            [&](Reptile*) { FAIL() << "bear is not reptile"; },
            [&](Amphibian*) { FAIL() << "bear is not amphibian"; },
            [&](Mammal* mammal) {
                EXPECT_EQ(mammal, bear.get());
                bear_matched_mammal = true;
            });
        EXPECT_TRUE(bear_matched_mammal);
    }
    {
        bool gecko_matched_reptile = false;
        Switch(
            gecko.get(),  //
            [&](Mammal*) { FAIL() << "gecko is not mammal"; },
            [&](Amphibian*) { FAIL() << "gecko is not amphibian"; },
            [&](Reptile* reptile) {
                EXPECT_EQ(reptile, gecko.get());
                gecko_matched_reptile = true;
            });
        EXPECT_TRUE(gecko_matched_reptile);
    }
}

TEST(Castable, SwitchWithUnusedDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        bool frog_matched_amphibian = false;
        Switch(
            frog.get(),  //
            [&](Reptile*) { FAIL() << "frog is not reptile"; },
            [&](Mammal*) { FAIL() << "frog is not mammal"; },
            [&](Amphibian* amphibian) {
                EXPECT_EQ(amphibian, frog.get());
                frog_matched_amphibian = true;
            },
            [&](Default) { FAIL() << "default should not have been selected"; });
        EXPECT_TRUE(frog_matched_amphibian);
    }
    {
        bool bear_matched_mammal = false;
        Switch(
            bear.get(),  //
            [&](Reptile*) { FAIL() << "bear is not reptile"; },
            [&](Amphibian*) { FAIL() << "bear is not amphibian"; },
            [&](Mammal* mammal) {
                EXPECT_EQ(mammal, bear.get());
                bear_matched_mammal = true;
            },
            [&](Default) { FAIL() << "default should not have been selected"; });
        EXPECT_TRUE(bear_matched_mammal);
    }
    {
        bool gecko_matched_reptile = false;
        Switch(
            gecko.get(),  //
            [&](Mammal*) { FAIL() << "gecko is not mammal"; },
            [&](Amphibian*) { FAIL() << "gecko is not amphibian"; },
            [&](Reptile* reptile) {
                EXPECT_EQ(reptile, gecko.get());
                gecko_matched_reptile = true;
            },
            [&](Default) { FAIL() << "default should not have been selected"; });
        EXPECT_TRUE(gecko_matched_reptile);
    }
}

TEST(Castable, SwitchDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        bool frog_matched_default = false;
        Switch(
            frog.get(),  //
            [&](Reptile*) { FAIL() << "frog is not reptile"; },
            [&](Mammal*) { FAIL() << "frog is not mammal"; },
            [&](Default) { frog_matched_default = true; });
        EXPECT_TRUE(frog_matched_default);
    }
    {
        bool bear_matched_default = false;
        Switch(
            bear.get(),  //
            [&](Reptile*) { FAIL() << "bear is not reptile"; },
            [&](Amphibian*) { FAIL() << "bear is not amphibian"; },
            [&](Default) { bear_matched_default = true; });
        EXPECT_TRUE(bear_matched_default);
    }
    {
        bool gecko_matched_default = false;
        Switch(
            gecko.get(),  //
            [&](Mammal*) { FAIL() << "gecko is not mammal"; },
            [&](Amphibian*) { FAIL() << "gecko is not amphibian"; },
            [&](Default) { gecko_matched_default = true; });
        EXPECT_TRUE(gecko_matched_default);
    }
}

TEST(Castable, SwitchMatchFirst) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    {
        bool frog_matched_animal = false;
        Switch(
            frog.get(),
            [&](Animal* animal) {
                EXPECT_EQ(animal, frog.get());
                frog_matched_animal = true;
            },
            [&](Amphibian*) { FAIL() << "animal should have been matched first"; });
        EXPECT_TRUE(frog_matched_animal);
    }
    {
        bool frog_matched_amphibian = false;
        Switch(
            frog.get(),
            [&](Amphibian* amphibain) {
                EXPECT_EQ(amphibain, frog.get());
                frog_matched_amphibian = true;
            },
            [&](Animal*) { FAIL() << "amphibian should have been matched first"; });
        EXPECT_TRUE(frog_matched_amphibian);
    }
}

TEST(Castable, SwitchReturnValueWithDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        const char* result = Switch(
            frog.get(),                              //
            [](Mammal*) { return "mammal"; },        //
            [](Amphibian*) { return "amphibian"; },  //
            [](Default) { return "unknown"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(std::string(result), "amphibian");
    }
    {
        const char* result = Switch(
            bear.get(),                              //
            [](Mammal*) { return "mammal"; },        //
            [](Amphibian*) { return "amphibian"; },  //
            [](Default) { return "unknown"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(std::string(result), "mammal");
    }
    {
        const char* result = Switch(
            gecko.get(),                             //
            [](Mammal*) { return "mammal"; },        //
            [](Amphibian*) { return "amphibian"; },  //
            [](Default) { return "unknown"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(std::string(result), "unknown");
    }
}

TEST(Castable, SwitchReturnValueWithoutDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        const char* result = Switch(
            frog.get(),                        //
            [](Mammal*) { return "mammal"; },  //
            [](Amphibian*) { return "amphibian"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(std::string(result), "amphibian");
    }
    {
        const char* result = Switch(
            bear.get(),                        //
            [](Mammal*) { return "mammal"; },  //
            [](Amphibian*) { return "amphibian"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(std::string(result), "mammal");
    }
    {
        auto* result = Switch(
            gecko.get(),                       //
            [](Mammal*) { return "mammal"; },  //
            [](Amphibian*) { return "amphibian"; });
        static_assert(std::is_same_v<decltype(result), const char*>);
        EXPECT_EQ(result, nullptr);
    }
}

TEST(Castable, SwitchInferPODReturnTypeWithDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto result = Switch(
            frog.get(),                       //
            [](Mammal*) { return 1; },        //
            [](Amphibian*) { return 2.0f; },  //
            [](Default) { return 3.0; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 2.0);
    }
    {
        auto result = Switch(
            bear.get(),                       //
            [](Mammal*) { return 1.0; },      //
            [](Amphibian*) { return 2.0f; },  //
            [](Default) { return 3; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 1.0);
    }
    {
        auto result = Switch(
            gecko.get(),                   //
            [](Mammal*) { return 1.0f; },  //
            [](Amphibian*) { return 2; },  //
            [](Default) { return 3.0; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 3.0);
    }
}

TEST(Castable, SwitchInferPODReturnTypeWithoutDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto result = Switch(
            frog.get(),                 //
            [](Mammal*) { return 1; },  //
            [](Amphibian*) { return 2.0f; });
        static_assert(std::is_same_v<decltype(result), float>);
        EXPECT_EQ(result, 2.0f);
    }
    {
        auto result = Switch(
            bear.get(),                    //
            [](Mammal*) { return 1.0f; },  //
            [](Amphibian*) { return 2; });
        static_assert(std::is_same_v<decltype(result), float>);
        EXPECT_EQ(result, 1.0f);
    }
    {
        auto result = Switch(
            gecko.get(),                  //
            [](Mammal*) { return 1.0; },  //
            [](Amphibian*) { return 2.0f; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 0.0);
    }
}

TEST(Castable, SwitchInferCastableReturnTypeWithDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto* result = Switch(
            frog.get(),                          //
            [](Mammal* p) { return p; },         //
            [](Amphibian*) { return nullptr; },  //
            [](Default) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), Mammal*>);
        EXPECT_EQ(result, nullptr);
    }
    {
        auto* result = Switch(
            bear.get(),                   //
            [](Mammal* p) { return p; },  //
            [](Amphibian* p) { return const_cast<const Amphibian*>(p); },
            [](Default) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), const Animal*>);
        EXPECT_EQ(result, bear.get());
    }
    {
        auto* result = Switch(
            gecko.get(),                     //
            [](Mammal* p) { return p; },     //
            [](Amphibian* p) { return p; },  //
            [](Default) -> CastableBase* { return nullptr; });
        static_assert(std::is_same_v<decltype(result), CastableBase*>);
        EXPECT_EQ(result, nullptr);
    }
}

TEST(Castable, SwitchInferCastableReturnTypeWithoutDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto* result = Switch(
            frog.get(),                   //
            [](Mammal* p) { return p; },  //
            [](Amphibian*) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), Mammal*>);
        EXPECT_EQ(result, nullptr);
    }
    {
        auto* result = Switch(
            bear.get(),                                                     //
            [](Mammal* p) { return p; },                                    //
            [](Amphibian* p) { return const_cast<const Amphibian*>(p); });  //
        static_assert(std::is_same_v<decltype(result), const Animal*>);
        EXPECT_EQ(result, bear.get());
    }
    {
        auto* result = Switch(
            gecko.get(),                  //
            [](Mammal* p) { return p; },  //
            [](Amphibian* p) { return p; });
        static_assert(std::is_same_v<decltype(result), Animal*>);
        EXPECT_EQ(result, nullptr);
    }
}

TEST(Castable, SwitchExplicitPODReturnTypeWithDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto result = Switch<double>(
            frog.get(),                       //
            [](Mammal*) { return 1; },        //
            [](Amphibian*) { return 2.0f; },  //
            [](Default) { return 3.0; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 2.0f);
    }
    {
        auto result = Switch<double>(
            bear.get(),                    //
            [](Mammal*) { return 1; },     //
            [](Amphibian*) { return 2; },  //
            [](Default) { return 3; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 1.0f);
    }
    {
        auto result = Switch<double>(
            gecko.get(),                      //
            [](Mammal*) { return 1.0f; },     //
            [](Amphibian*) { return 2.0f; },  //
            [](Default) { return 3.0f; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 3.0f);
    }
}

TEST(Castable, SwitchExplicitPODReturnTypeWithoutDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto result = Switch<double>(
            frog.get(),                 //
            [](Mammal*) { return 1; },  //
            [](Amphibian*) { return 2.0f; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 2.0f);
    }
    {
        auto result = Switch<double>(
            bear.get(),                    //
            [](Mammal*) { return 1.0f; },  //
            [](Amphibian*) { return 2; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 1.0f);
    }
    {
        auto result = Switch<double>(
            gecko.get(),                  //
            [](Mammal*) { return 1.0; },  //
            [](Amphibian*) { return 2.0f; });
        static_assert(std::is_same_v<decltype(result), double>);
        EXPECT_EQ(result, 0.0);
    }
}

TEST(Castable, SwitchExplicitCastableReturnTypeWithDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto* result = Switch<Animal>(
            frog.get(),                          //
            [](Mammal* p) { return p; },         //
            [](Amphibian*) { return nullptr; },  //
            [](Default) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), Animal*>);
        EXPECT_EQ(result, nullptr);
    }
    {
        auto* result = Switch<CastableBase>(
            bear.get(),                   //
            [](Mammal* p) { return p; },  //
            [](Amphibian* p) { return const_cast<const Amphibian*>(p); },
            [](Default) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), const CastableBase*>);
        EXPECT_EQ(result, bear.get());
    }
    {
        auto* result = Switch<const Animal>(
            gecko.get(),                     //
            [](Mammal* p) { return p; },     //
            [](Amphibian* p) { return p; },  //
            [](Default) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), const Animal*>);
        EXPECT_EQ(result, nullptr);
    }
}

TEST(Castable, SwitchExplicitCastableReturnTypeWithoutDefault) {
    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    std::unique_ptr<Animal> bear = std::make_unique<Bear>();
    std::unique_ptr<Animal> gecko = std::make_unique<Gecko>();
    {
        auto* result = Switch<Animal>(
            frog.get(),                   //
            [](Mammal* p) { return p; },  //
            [](Amphibian*) { return nullptr; });
        static_assert(std::is_same_v<decltype(result), Animal*>);
        EXPECT_EQ(result, nullptr);
    }
    {
        auto* result = Switch<CastableBase>(
            bear.get(),                                                     //
            [](Mammal* p) { return p; },                                    //
            [](Amphibian* p) { return const_cast<const Amphibian*>(p); });  //
        static_assert(std::is_same_v<decltype(result), const CastableBase*>);
        EXPECT_EQ(result, bear.get());
    }
    {
        auto* result = Switch<const Animal*>(
            gecko.get(),                  //
            [](Mammal* p) { return p; },  //
            [](Amphibian* p) { return p; });
        static_assert(std::is_same_v<decltype(result), const Animal*>);
        EXPECT_EQ(result, nullptr);
    }
}

TEST(Castable, SwitchNull) {
    Animal* null = nullptr;
    Switch(
        null,  //
        [&](Amphibian*) { FAIL() << "should not be called"; },
        [&](Animal*) { FAIL() << "should not be called"; });
}

TEST(Castable, SwitchNullNoDefault) {
    Animal* null = nullptr;
    bool default_called = false;
    Switch(
        null,  //
        [&](Amphibian*) { FAIL() << "should not be called"; },
        [&](Animal*) { FAIL() << "should not be called"; },
        [&](Default) { default_called = true; });
    EXPECT_TRUE(default_called);
}

TEST(Castable, SwitchReturnNoDefaultInitializer) {
    struct Object {
        explicit Object(int v) : value(v) {}
        int value;
    };

    std::unique_ptr<Animal> frog = std::make_unique<Frog>();
    {
        auto result = Switch(
            frog.get(),                            //
            [](Mammal*) { return Object(1); },     //
            [](Amphibian*) { return Object(2); },  //
            [](Default) { return Object(3); });
        static_assert(std::is_same_v<decltype(result), Object>);
        EXPECT_EQ(result.value, 2);
    }
    {
        auto result = Switch(
            frog.get(),                         //
            [](Mammal*) { return Object(1); },  //
            [](Default) { return Object(3); });
        static_assert(std::is_same_v<decltype(result), Object>);
        EXPECT_EQ(result.value, 3);
    }
}

// 0 1
//  2
//
//
using HCT_2 = detail::HashCodeTree<void(Mammal*), void(Amphibian*)>;
static_assert(HCT_2::kNumCasesRounded == 2u);
static_assert(HCT_2::kNumLevels == 2u);
static_assert(HCT_2::kCount == 3u);
static_assert(HCT_2::kLevelOffsets[0] == 0u);
static_assert(HCT_2::kLevelOffsets[1] == 2u);
static_assert(HCT_2::kLevelOffsets.size() == 2u);
static_assert(HCT_2::kValues[0] == TypeInfo::HashCodeOf<Mammal>());
static_assert(HCT_2::kValues[1] == TypeInfo::HashCodeOf<Amphibian>());
static_assert(HCT_2::kValues[2] ==
              (TypeInfo::HashCodeOf<Mammal>() | TypeInfo::HashCodeOf<Amphibian>()));
static_assert(HCT_2::kValues.size() == 3u);

// 0 1 2 X
//  4   5
//    6
//
using HCT_3 = detail::HashCodeTree<void(Frog*), void(Gecko*), void(Reptile*)>;
static_assert(HCT_3::kNumCasesRounded == 4u);
static_assert(HCT_3::kNumLevels == 3u);
static_assert(HCT_3::kCount == 7u);
static_assert(HCT_3::kLevelOffsets[0] == 0u);
static_assert(HCT_3::kLevelOffsets[1] == 4u);
static_assert(HCT_3::kLevelOffsets[2] == 6u);
static_assert(HCT_3::kLevelOffsets.size() == 3u);
static_assert(HCT_3::kValues[0] == TypeInfo::HashCodeOf<Frog>());
static_assert(HCT_3::kValues[1] == TypeInfo::HashCodeOf<Gecko>());
static_assert(HCT_3::kValues[2] == TypeInfo::HashCodeOf<Reptile>());
static_assert(HCT_3::kValues[3] == 0);
static_assert(HCT_3::kValues[4] == (TypeInfo::HashCodeOf<Frog>() | TypeInfo::HashCodeOf<Gecko>()));
static_assert(HCT_3::kValues[5] == TypeInfo::HashCodeOf<Reptile>());
static_assert(HCT_3::kValues[6] == (TypeInfo::HashCodeOf<Frog>() | TypeInfo::HashCodeOf<Gecko>() |
                                    TypeInfo::HashCodeOf<Reptile>()));
static_assert(HCT_3::kValues.size() == 7u);

// 0 1 2 3
//  4   5
//    6
//
using HCT_4 = detail::HashCodeTree<void(Bear*), void(Frog*), void(Gecko*), void(Reptile*)>;
static_assert(HCT_4::kNumCasesRounded == 4u);
static_assert(HCT_4::kNumLevels == 3u);
static_assert(HCT_4::kCount == 7u);
static_assert(HCT_4::kLevelOffsets[0] == 0u);
static_assert(HCT_4::kLevelOffsets[1] == 4u);
static_assert(HCT_4::kLevelOffsets[2] == 6u);
static_assert(HCT_4::kLevelOffsets.size() == 3u);
static_assert(HCT_4::kValues[0] == TypeInfo::HashCodeOf<Bear>());
static_assert(HCT_4::kValues[1] == TypeInfo::HashCodeOf<Frog>());
static_assert(HCT_4::kValues[2] == TypeInfo::HashCodeOf<Gecko>());
static_assert(HCT_4::kValues[3] == TypeInfo::HashCodeOf<Reptile>());
static_assert(HCT_4::kValues[4] == (TypeInfo::HashCodeOf<Bear>() | TypeInfo::HashCodeOf<Frog>()));
static_assert(HCT_4::kValues[5] ==
              (TypeInfo::HashCodeOf<Gecko>() | TypeInfo::HashCodeOf<Reptile>()));
static_assert(HCT_4::kValues[6] ==
              (TypeInfo::HashCodeOf<Bear>() | TypeInfo::HashCodeOf<Frog>() |
               TypeInfo::HashCodeOf<Gecko>() | TypeInfo::HashCodeOf<Reptile>()));
static_assert(HCT_4::kValues.size() == 7u);

// 0 1 2 3 4 X X X
//  8   9   10   X
//    12       13
//        14
using HCT_5 = detail::
    HashCodeTree<void(Reptile*), void(Gecko*), void(Lizard*), void(Mammal*), void(Amphibian*)>;
static_assert(HCT_5::kNumCasesRounded == 8u);
static_assert(HCT_5::kNumLevels == 4u);
static_assert(HCT_5::kCount == 15u);
static_assert(HCT_5::kLevelOffsets[0] == 0u);
static_assert(HCT_5::kLevelOffsets[1] == 8u);
static_assert(HCT_5::kLevelOffsets[2] == 12u);
static_assert(HCT_5::kLevelOffsets[3] == 14u);
static_assert(HCT_5::kLevelOffsets.size() == 4u);
static_assert(HCT_5::kValues[0] == TypeInfo::HashCodeOf<Reptile>());
static_assert(HCT_5::kValues[1] == TypeInfo::HashCodeOf<Gecko>());
static_assert(HCT_5::kValues[2] == TypeInfo::HashCodeOf<Lizard>());
static_assert(HCT_5::kValues[3] == TypeInfo::HashCodeOf<Mammal>());
static_assert(HCT_5::kValues[4] == TypeInfo::HashCodeOf<Amphibian>());
static_assert(HCT_5::kValues[5] == 0);
static_assert(HCT_5::kValues[6] == 0);
static_assert(HCT_5::kValues[7] == 0);
static_assert(HCT_5::kValues[8] ==
              (TypeInfo::HashCodeOf<Reptile>() | TypeInfo::HashCodeOf<Gecko>()));
static_assert(HCT_5::kValues[9] ==
              (TypeInfo::HashCodeOf<Lizard>() | TypeInfo::HashCodeOf<Mammal>()));
static_assert(HCT_5::kValues[10] == TypeInfo::HashCodeOf<Amphibian>());
static_assert(HCT_5::kValues[12] ==
              (TypeInfo::HashCodeOf<Reptile>() | TypeInfo::HashCodeOf<Gecko>() |
               TypeInfo::HashCodeOf<Lizard>() | TypeInfo::HashCodeOf<Mammal>()));
static_assert(HCT_5::kValues[13] == TypeInfo::HashCodeOf<Amphibian>());
static_assert(HCT_5::kValues[14] ==
              (TypeInfo::HashCodeOf<Reptile>() | TypeInfo::HashCodeOf<Gecko>() |
               TypeInfo::HashCodeOf<Lizard>() | TypeInfo::HashCodeOf<Mammal>() |
               TypeInfo::HashCodeOf<Amphibian>()));
static_assert(HCT_5::kValues.size() == 15u);

}  // namespace

TINT_INSTANTIATE_TYPEINFO(Animal);
TINT_INSTANTIATE_TYPEINFO(Amphibian);
TINT_INSTANTIATE_TYPEINFO(Mammal);
TINT_INSTANTIATE_TYPEINFO(Reptile);
TINT_INSTANTIATE_TYPEINFO(Frog);
TINT_INSTANTIATE_TYPEINFO(Bear);
TINT_INSTANTIATE_TYPEINFO(Lizard);
TINT_INSTANTIATE_TYPEINFO(Gecko);

}  // namespace tint

extern "C" const char* ASwitchCase(tint::Animal* animal);

extern "C" const char* ASwitchCase(tint::Animal* animal) {
    return tint::Switch(
        animal,                                        //
        [](tint::Mammal*) { return "mammal"; },        //
        [](tint::Amphibian*) { return "amphibian"; },  //
        [](tint::Default) { return "unknown"; });
}
