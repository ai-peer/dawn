// Copyright 2018 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_SHADERMODULEVK_H_
#define SRC_DAWN_NATIVE_VULKAN_SHADERMODULEVK_H_

#include <memory>
#include <mutex>
// TODO(https://crbug.com/dawn/1379) Update cpplint and remove NOLINT
#include <optional>  // NOLINT(build/include_order))
#include <unordered_map>
#include <utility>
#include <vector>

#include "dawn/native/BlobCache.h"
#include "dawn/native/ShaderModule.h"

#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Error.h"

namespace dawn::native::vulkan {

class Device;
class PipelineLayout;

class ShaderModule final : public ShaderModuleBase {
  public:
    // Spirv is a wrapper around a blob of data which is passed to
    // vkCreateShaderModule. The size in bytes must be a multiple of four.
    // It uses type erasure so that it can be constructed from multiple
    // data types, taking ownership of the memory, and provide access to
    // the data without copying.
    class Spirv {
      public:
        Spirv();
        ~Spirv();
        explicit Spirv(std::vector<uint32_t> data);
        explicit Spirv(CachedBlob data);
        Spirv(const Spirv& rhs);
        Spirv(Spirv&& rhs);

        Spirv& operator=(const Spirv& rhs);
        Spirv& operator=(Spirv&& rhs);
        Spirv& operator=(std::vector<uint32_t> data);
        Spirv& operator=(CachedBlob data);

        const uint32_t* Data() const;
        size_t SizeInBytes() const;

        // Iterator compatibility.
        const uint32_t* begin() const;
        const uint32_t* end() const;
        size_t size() const;

      private:
        uint32_t* mData = nullptr;
        size_t mDataCount = 0;
        std::function<void()> mDataDeleter = []() {};
    };

    using ModuleAndSpirv = std::pair<VkShaderModule, const Spirv*>;

    static ResultOrError<Ref<ShaderModule>> Create(Device* device,
                                                   const ShaderModuleDescriptor* descriptor,
                                                   ShaderModuleParseResult* parseResult,
                                                   OwnedCompilationMessages* compilationMessages);

    ResultOrError<ModuleAndSpirv> GetHandleAndSpirv(const char* entryPointName,
                                                    const PipelineLayout* layout);

  private:
    ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor);
    ~ShaderModule() override;
    MaybeError Initialize(ShaderModuleParseResult* parseResult,
                          OwnedCompilationMessages* compilationMessages);
    void DestroyImpl() override;

    // New handles created by GetHandleAndSpirv at pipeline creation time.
    class ConcurrentTransformedShaderModuleCache {
      public:
        explicit ConcurrentTransformedShaderModuleCache(Device* device);
        ~ConcurrentTransformedShaderModuleCache();

        std::optional<ModuleAndSpirv> Find(const PipelineLayoutEntryPointPair& key);
        ModuleAndSpirv AddOrGet(const PipelineLayoutEntryPointPair& key,
                                VkShaderModule module,
                                Spirv&& spirv);

      private:
        using Entry = std::pair<VkShaderModule, std::unique_ptr<Spirv>>;

        Device* mDevice;
        std::mutex mMutex;
        std::
            unordered_map<PipelineLayoutEntryPointPair, Entry, PipelineLayoutEntryPointPairHashFunc>
                mTransformedShaderModuleCache;
    };
    std::unique_ptr<ConcurrentTransformedShaderModuleCache> mTransformedShaderModuleCache;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_SHADERMODULEVK_H_
