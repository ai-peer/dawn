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

#ifndef SRC_TINT_IR_TRANSFORM_SHADER_IO_H_
#define SRC_TINT_IR_TRANSFORM_SHADER_IO_H_

#include "src/tint/ir/transform/transform.h"

namespace tint::ir::transform {

/// ShaderIO is a transform that modifies an entry point function's parameters and return value to
/// prepare them for backend codegen.
class ShaderIO final : public utils::Castable<ShaderIO, Transform> {
  public:
    /// Backend is an enumerator of the different backend targets.
    enum class Backend {
        /// Target SPIR-V (using global variables).
        kSpirv,
    };

    /// Configuration options for the transform.
    struct Config final : public utils::Castable<Config, Data> {
        /// Constructor
        /// @param backend the backend to target
        explicit Config(Backend backend);

        /// Copy constructor
        // Config(const Config&);

        /// Destructor
        ~Config() override;

        /// The backend to target.
        const Backend backend;
    };

    /// Constructor
    ShaderIO();
    /// Destructor
    ~ShaderIO() override;

    /// @copydoc Transform::Run
    void Run(ir::Module* module, const DataMap& inputs, DataMap& outputs) const override;

  private:
    struct State;
};

}  // namespace tint::ir::transform

#endif  // SRC_TINT_IR_TRANSFORM_SHADER_IO_H_
