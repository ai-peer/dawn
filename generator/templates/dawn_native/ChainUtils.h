// Copyright 2021 The Dawn Authors
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

#ifndef DAWNNATIVE_CHAIN_UTILS_H_
#define DAWNNATIVE_CHAIN_UTILS_H_

#include "dawn_native/dawn_platform.h"
#include "dawn_native/Error.h"

namespace dawn_native {
	{% for value in types["s type"].values %}
		{% if value.valid %}
			void FindInChain(const ChainedStruct* chain, const {{value.name.CamelCase()}}** out);
		{% endif %}
	{% endfor %}

	// Verifies that |chain| only contains ChainedStructs of types enumerated in
	// |oneOfConstraints| and contains no duplicate sTypes. Each vector in
	// |oneOfConstraints| defines a set of sTypes that cannot coexist in the same chain.
	// For example:
	//   ValidateSTypes(chain, { { ShaderModuleSPIRVDescriptor, ShaderModuleWGSLDescriptor } }))
	//   ValidateSTypes(chain, { { Extension1 }, { Extension2 } })
    MaybeError ValidateSTypes(const ChainedStruct* chain,
                              std::vector<std::vector<wgpu::SType>> oneOfConstraints);

}  // namespace dawn_native

#endif  // DAWNNATIVE_CHAIN_UTILS_H_
