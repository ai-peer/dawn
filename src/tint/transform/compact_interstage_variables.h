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

#ifndef SRC_TINT_TRANSFORM_COMPACT_INTERSTAGE_VARIABLES_H_
#define SRC_TINT_TRANSFORM_COMPACT_INTERSTAGE_VARIABLES_H_

#include <array>
#include <string>
#include <unordered_map>

#include "src/tint/sem/binding_point.h"
#include "src/tint/transform/transform.h"
#include "src/tint/utils/bitset.h"

namespace tint::transform {

/// CompactInterstageVariables is a Transform that renames all the symbols in a program.
class CompactInterstageVariables final : public Castable<CompactInterstageVariables, Transform> {
  public:
    /// Configuration options for the transform
    struct Config final : public Castable<Config, Data> {
        /// Constructor
        Config();

        /// Copy constructor
        Config(const Config&);

        /// Destructor
        ~Config() override;

        /// Assignment operator
        /// @returns this Config
        Config& operator=(const Config&);

        // /// The map of override identifier to the override value.
        // /// The value is always a double coming into the transform and will be
        // /// converted to the correct type through and initializer.
        // std::array<tint::writer::hlsl::InterstageVariableInfo, 16> interstage_variables;

        // std::vector<sem::InterStageVariableInfo> interstage_variables;

        utils::Bitset<16> interstage_locations;

        // /// Reflect the fields of this class so that it can be used by tint::ForeachField()
        TINT_REFLECT(interstage_variables);
    };
    // /// Data is outputted by the CompactInterstageVariables transform.
    // /// Data holds information about shader usage and constant buffer offsets.
    // struct Data final : public Castable<Data, transform::Data> {
    //     /// Remappings is a map of old symbol name to new symbol name
    //     using Remappings = std::unordered_map<std::string, std::string>;

    //     /// Constructor
    //     /// @param remappings the symbol remappings
    //     explicit Data(Remappings&& remappings);

    //     /// Copy constructor
    //     Data(const Data&);

    //     /// Destructor
    //     ~Data() override;

    //     /// A map of old symbol name to new symbol name
    //     const Remappings remappings;
    // };

    // /// Target is an enumerator of rename targets that can be used
    // enum class Target {
    //     /// Rename every symbol.
    //     kAll,
    //     /// Only rename symbols that are reserved keywords in GLSL.
    //     kGlslKeywords,
    //     /// Only rename symbols that are reserved keywords in HLSL.
    //     kHlslKeywords,
    //     /// Only rename symbols that are reserved keywords in MSL.
    //     kMslKeywords,
    // };

    // /// Optional configuration options for the transform.
    // /// If omitted, then the renamer will use Target::kAll.
    // struct Config final : public Castable<Config, transform::Data> {
    //     /// Constructor
    //     /// @param tgt the targets to rename
    //     /// @param keep_unicode if false, symbols with non-ascii code-points are
    //     /// renamed
    //     explicit Config(Target tgt, bool keep_unicode = false);

    //     /// Copy constructor
    //     Config(const Config&);

    //     /// Destructor
    //     ~Config() override;

    //     /// The targets to rename
    //     Target const target = Target::kAll;

    //     /// If false, symbols with non-ascii code-points are renamed.
    //     bool preserve_unicode = false;
    // };

    /// Constructor using a the configuration provided in the input Data
    CompactInterstageVariables();

    /// Destructor
    ~CompactInterstageVariables() override;

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_COMPACT_INTERSTAGE_VARIABLES_H_
