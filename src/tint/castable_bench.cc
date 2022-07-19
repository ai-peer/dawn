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

#include "bench/benchmark.h"

namespace tint {
namespace {

struct Base : public tint::Castable<Base> {};
struct A : public tint::Castable<A, Base> {};
struct AA : public tint::Castable<AA, A> {};
struct AAA : public tint::Castable<AAA, AA> {};
struct AAB : public tint::Castable<AAB, AA> {};
struct AAC : public tint::Castable<AAC, AA> {};
struct AB : public tint::Castable<AB, A> {};
struct ABA : public tint::Castable<ABA, AB> {};
struct ABB : public tint::Castable<ABB, AB> {};
struct ABC : public tint::Castable<ABC, AB> {};
struct AC : public tint::Castable<AC, A> {};
struct ACA : public tint::Castable<ACA, AC> {};
struct ACB : public tint::Castable<ACB, AC> {};
struct ACC : public tint::Castable<ACC, AC> {};
struct B : public tint::Castable<B, Base> {};
struct BA : public tint::Castable<BA, B> {};
struct BAA : public tint::Castable<BAA, BA> {};
struct BAB : public tint::Castable<BAB, BA> {};
struct BAC : public tint::Castable<BAC, BA> {};
struct BB : public tint::Castable<BB, B> {};
struct BBA : public tint::Castable<BBA, BB> {};
struct BBB : public tint::Castable<BBB, BB> {};
struct BBC : public tint::Castable<BBC, BB> {};
struct BC : public tint::Castable<BC, B> {};
struct BCA : public tint::Castable<BCA, BC> {};
struct BCB : public tint::Castable<BCB, BC> {};
struct BCC : public tint::Castable<BCC, BC> {};
struct C : public tint::Castable<C, Base> {};
struct CA : public tint::Castable<CA, C> {};
struct CAA : public tint::Castable<CAA, CA> {};
struct CAB : public tint::Castable<CAB, CA> {};
struct CAC : public tint::Castable<CAC, CA> {};
struct CB : public tint::Castable<CB, C> {};
struct CBA : public tint::Castable<CBA, CB> {};
struct CBB : public tint::Castable<CBB, CB> {};
struct CBC : public tint::Castable<CBC, CB> {};
struct CC : public tint::Castable<CC, C> {};
struct CCA : public tint::Castable<CCA, CC> {};
struct CCB : public tint::Castable<CCB, CC> {};
struct CCC : public tint::Castable<CCC, CC> {};

using AllTypes = std::tuple<Base,
                            A,
                            AA,
                            AAA,
                            AAB,
                            AAC,
                            AB,
                            ABA,
                            ABB,
                            ABC,
                            AC,
                            ACA,
                            ACB,
                            ACC,
                            B,
                            BA,
                            BAA,
                            BAB,
                            BAC,
                            BB,
                            BBA,
                            BBB,
                            BBC,
                            BC,
                            BCA,
                            BCB,
                            BCC,
                            C,
                            CA,
                            CAA,
                            CAB,
                            CAC,
                            CB,
                            CBA,
                            CBB,
                            CBC,
                            CC,
                            CCA,
                            CCB,
                            CCC>;

std::vector<std::unique_ptr<Base>> MakeObjects() {
    std::vector<std::unique_ptr<Base>> out;
    out.emplace_back(std::make_unique<Base>());
    out.emplace_back(std::make_unique<A>());
    out.emplace_back(std::make_unique<AA>());
    out.emplace_back(std::make_unique<AAA>());
    out.emplace_back(std::make_unique<AAB>());
    out.emplace_back(std::make_unique<AAC>());
    out.emplace_back(std::make_unique<AB>());
    out.emplace_back(std::make_unique<ABA>());
    out.emplace_back(std::make_unique<ABB>());
    out.emplace_back(std::make_unique<ABC>());
    out.emplace_back(std::make_unique<AC>());
    out.emplace_back(std::make_unique<ACA>());
    out.emplace_back(std::make_unique<ACB>());
    out.emplace_back(std::make_unique<ACC>());
    out.emplace_back(std::make_unique<B>());
    out.emplace_back(std::make_unique<BA>());
    out.emplace_back(std::make_unique<BAA>());
    out.emplace_back(std::make_unique<BAB>());
    out.emplace_back(std::make_unique<BAC>());
    out.emplace_back(std::make_unique<BB>());
    out.emplace_back(std::make_unique<BBA>());
    out.emplace_back(std::make_unique<BBB>());
    out.emplace_back(std::make_unique<BBC>());
    out.emplace_back(std::make_unique<BC>());
    out.emplace_back(std::make_unique<BCA>());
    out.emplace_back(std::make_unique<BCB>());
    out.emplace_back(std::make_unique<BCC>());
    out.emplace_back(std::make_unique<C>());
    out.emplace_back(std::make_unique<CA>());
    out.emplace_back(std::make_unique<CAA>());
    out.emplace_back(std::make_unique<CAB>());
    out.emplace_back(std::make_unique<CAC>());
    out.emplace_back(std::make_unique<CB>());
    out.emplace_back(std::make_unique<CBA>());
    out.emplace_back(std::make_unique<CBB>());
    out.emplace_back(std::make_unique<CBC>());
    out.emplace_back(std::make_unique<CC>());
    out.emplace_back(std::make_unique<CCA>());
    out.emplace_back(std::make_unique<CCB>());
    out.emplace_back(std::make_unique<CCC>());
    return out;
}

void CastableLargeSwitch(::benchmark::State& state) {
    auto objects = MakeObjects();
    size_t i = 0;
    for (auto _ : state) {
        auto* object = objects[i % objects.size()].get();
        Switch(
            object,  //
            [&](const AAA*) { ::benchmark::DoNotOptimize(i += 40); },
            [&](const AAB*) { ::benchmark::DoNotOptimize(i += 50); },
            [&](const AAC*) { ::benchmark::DoNotOptimize(i += 60); },
            [&](const ABA*) { ::benchmark::DoNotOptimize(i += 80); },
            [&](const ABB*) { ::benchmark::DoNotOptimize(i += 90); },
            [&](const ABC*) { ::benchmark::DoNotOptimize(i += 100); },
            [&](const ACA*) { ::benchmark::DoNotOptimize(i += 120); },
            [&](const ACB*) { ::benchmark::DoNotOptimize(i += 130); },
            [&](const ACC*) { ::benchmark::DoNotOptimize(i += 140); },
            [&](const BAA*) { ::benchmark::DoNotOptimize(i += 170); },
            [&](const BAB*) { ::benchmark::DoNotOptimize(i += 180); },
            [&](const BAC*) { ::benchmark::DoNotOptimize(i += 190); },
            [&](const BBA*) { ::benchmark::DoNotOptimize(i += 210); },
            [&](const BBB*) { ::benchmark::DoNotOptimize(i += 220); },
            [&](const BBC*) { ::benchmark::DoNotOptimize(i += 230); },
            [&](const BCA*) { ::benchmark::DoNotOptimize(i += 250); },
            [&](const BCB*) { ::benchmark::DoNotOptimize(i += 260); },
            [&](const BCC*) { ::benchmark::DoNotOptimize(i += 270); },
            [&](const CA*) { ::benchmark::DoNotOptimize(i += 290); },
            [&](const CAA*) { ::benchmark::DoNotOptimize(i += 300); },
            [&](const CAB*) { ::benchmark::DoNotOptimize(i += 310); },
            [&](const CAC*) { ::benchmark::DoNotOptimize(i += 320); },
            [&](const CBA*) { ::benchmark::DoNotOptimize(i += 340); },
            [&](const CBB*) { ::benchmark::DoNotOptimize(i += 350); },
            [&](const CBC*) { ::benchmark::DoNotOptimize(i += 360); },
            [&](const CCA*) { ::benchmark::DoNotOptimize(i += 380); },
            [&](const CCB*) { ::benchmark::DoNotOptimize(i += 390); },
            [&](const CCC*) { ::benchmark::DoNotOptimize(i += 400); },
            [&](Default) { ::benchmark::DoNotOptimize(i += 123); });
        i = (i * 31) ^ (i << 5);
    }
}

BENCHMARK(CastableLargeSwitch);

void CastableMediumSwitch(::benchmark::State& state) {
    auto objects = MakeObjects();
    size_t i = 0;
    for (auto _ : state) {
        auto* object = objects[i % objects.size()].get();
        Switch(
            object,  //
            [&](const ACB*) { ::benchmark::DoNotOptimize(i += 130); },
            [&](const BAA*) { ::benchmark::DoNotOptimize(i += 170); },
            [&](const BAB*) { ::benchmark::DoNotOptimize(i += 180); },
            [&](const BBA*) { ::benchmark::DoNotOptimize(i += 210); },
            [&](const BBB*) { ::benchmark::DoNotOptimize(i += 220); },
            [&](const CAA*) { ::benchmark::DoNotOptimize(i += 300); },
            [&](const CCA*) { ::benchmark::DoNotOptimize(i += 380); },
            [&](const CCB*) { ::benchmark::DoNotOptimize(i += 390); },
            [&](const CCC*) { ::benchmark::DoNotOptimize(i += 400); },
            [&](Default) { ::benchmark::DoNotOptimize(i += 123); });
        i = (i * 31) ^ (i << 5);
    }
}

BENCHMARK(CastableMediumSwitch);

void CastableSmallSwitch(::benchmark::State& state) {
    auto objects = MakeObjects();
    size_t i = 0;
    for (auto _ : state) {
        auto* object = objects[i % objects.size()].get();
        Switch(
            object,  //
            [&](const AAB*) { ::benchmark::DoNotOptimize(i += 30); },
            [&](const CAC*) { ::benchmark::DoNotOptimize(i += 290); },
            [&](const CAA*) { ::benchmark::DoNotOptimize(i += 300); });
        i = (i * 31) ^ (i << 5);
    }
}

BENCHMARK(CastableSmallSwitch);

}  // namespace
}  // namespace tint

TINT_INSTANTIATE_TYPEINFO(tint::Base, false);
TINT_INSTANTIATE_TYPEINFO(tint::A, false);
TINT_INSTANTIATE_TYPEINFO(tint::AA, false);
TINT_INSTANTIATE_TYPEINFO(tint::AAA, true);
TINT_INSTANTIATE_TYPEINFO(tint::AAB, true);
TINT_INSTANTIATE_TYPEINFO(tint::AAC, true);
TINT_INSTANTIATE_TYPEINFO(tint::AB, false);
TINT_INSTANTIATE_TYPEINFO(tint::ABA, true);
TINT_INSTANTIATE_TYPEINFO(tint::ABB, true);
TINT_INSTANTIATE_TYPEINFO(tint::ABC, true);
TINT_INSTANTIATE_TYPEINFO(tint::AC, false);
TINT_INSTANTIATE_TYPEINFO(tint::ACA, true);
TINT_INSTANTIATE_TYPEINFO(tint::ACB, true);
TINT_INSTANTIATE_TYPEINFO(tint::ACC, true);
TINT_INSTANTIATE_TYPEINFO(tint::B, false);
TINT_INSTANTIATE_TYPEINFO(tint::BA, false);
TINT_INSTANTIATE_TYPEINFO(tint::BAA, true);
TINT_INSTANTIATE_TYPEINFO(tint::BAB, true);
TINT_INSTANTIATE_TYPEINFO(tint::BAC, true);
TINT_INSTANTIATE_TYPEINFO(tint::BB, false);
TINT_INSTANTIATE_TYPEINFO(tint::BBA, true);
TINT_INSTANTIATE_TYPEINFO(tint::BBB, true);
TINT_INSTANTIATE_TYPEINFO(tint::BBC, true);
TINT_INSTANTIATE_TYPEINFO(tint::BC, false);
TINT_INSTANTIATE_TYPEINFO(tint::BCA, true);
TINT_INSTANTIATE_TYPEINFO(tint::BCB, true);
TINT_INSTANTIATE_TYPEINFO(tint::BCC, true);
TINT_INSTANTIATE_TYPEINFO(tint::C, false);
TINT_INSTANTIATE_TYPEINFO(tint::CA, false);
TINT_INSTANTIATE_TYPEINFO(tint::CAA, true);
TINT_INSTANTIATE_TYPEINFO(tint::CAB, true);
TINT_INSTANTIATE_TYPEINFO(tint::CAC, true);
TINT_INSTANTIATE_TYPEINFO(tint::CB, false);
TINT_INSTANTIATE_TYPEINFO(tint::CBA, true);
TINT_INSTANTIATE_TYPEINFO(tint::CBB, true);
TINT_INSTANTIATE_TYPEINFO(tint::CBC, true);
TINT_INSTANTIATE_TYPEINFO(tint::CC, false);
TINT_INSTANTIATE_TYPEINFO(tint::CCA, true);
TINT_INSTANTIATE_TYPEINFO(tint::CCB, true);
TINT_INSTANTIATE_TYPEINFO(tint::CCC, true);
