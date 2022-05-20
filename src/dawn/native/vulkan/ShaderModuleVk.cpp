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

#include "dawn/native/vulkan/ShaderModuleVk.h"

#include <spirv-tools/libspirv.hpp>

#include <map>

#include "dawn/native/CacheRequestBuilder.h"
#include "dawn/native/LogSink.h"
#include "dawn/native/SpirvValidation.h"
#include "dawn/native/TintUtils.h"
#include "dawn/native/vulkan/BindGroupLayoutVk.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/PipelineLayoutVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/native/vulkan/VulkanError.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"
#include "tint/tint.h"

namespace dawn::native::vulkan {

ShaderModule::Spirv::Spirv() = default;
ShaderModule::Spirv::~Spirv() {
    if (mDataDeleter) {
        mDataDeleter();
    }
}
ShaderModule::Spirv::Spirv(std::vector<uint32_t> data) : Spirv() {
    *this = std::move(data);
}

ShaderModule::Spirv::Spirv(CachedBlob data) : Spirv() {
    *this = std::move(data);
}

ShaderModule::Spirv::Spirv(const Spirv& rhs) : Spirv() {
    *this = rhs;
}

ShaderModule::Spirv::Spirv(Spirv&& rhs) : Spirv() {
    *this = std::forward<Spirv>(rhs);
}

ShaderModule::Spirv& ShaderModule::Spirv::operator=(const Spirv& rhs) {
    std::vector<uint32_t> data(rhs.mData, rhs.mData + rhs.mDataCount);
    return Spirv::operator=(std::move(data));
}

ShaderModule::Spirv& ShaderModule::Spirv::operator=(Spirv&& rhs) {
    mData = rhs.mData;
    mDataCount = rhs.mDataCount;
    mDataDeleter = std::move(rhs.mDataDeleter);
    return *this;
}

ShaderModule::Spirv& ShaderModule::Spirv::operator=(std::vector<uint32_t> data) {
    mData = data.data();
    mDataCount = data.size();
    mDataDeleter = [data = std::move(data)]() { auto local = std::move(data); };
    return *this;
}

ShaderModule::Spirv& ShaderModule::Spirv::operator=(CachedBlob data) {
    mDataCount = data.Size() / sizeof(uint32_t);
    // We should never have stored a blob of the wrong size.
    ASSERT(mDataCount * sizeof(uint32_t) == data.Size());

    if (IsPtrAligned(data.Data(), alignof(uint32_t))) {
        mData = reinterpret_cast<uint32_t*>(data.Data());
        mDataDeleter = [data = new CachedBlob(std::move(data))]() { delete data; };
        return *this;
    }

    // Unaligned data. Copy into std::vector<uint32_t>.
    std::vector<uint32_t> vec(mDataCount);
    memcpy(vec.data(), data.Data(), sizeof(uint32_t) * mDataCount);
    return Spirv::operator=(std::move(vec));
}

const uint32_t* ShaderModule::Spirv::Data() const {
    return mData;
}

size_t ShaderModule::Spirv::SizeInBytes() const {
    return mDataCount * sizeof(uint32_t);
}

const uint32_t* ShaderModule::Spirv::begin() const {
    return Data();
}

const uint32_t* ShaderModule::Spirv::end() const {
    return Data() + mDataCount;
}

size_t ShaderModule::Spirv::size() const {
    return mDataCount;
}

ShaderModule::ConcurrentTransformedShaderModuleCache::ConcurrentTransformedShaderModuleCache(
    Device* device)
    : mDevice(device) {}

ShaderModule::ConcurrentTransformedShaderModuleCache::~ConcurrentTransformedShaderModuleCache() {
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& [_, moduleAndSpirv] : mTransformedShaderModuleCache) {
        mDevice->GetFencedDeleter()->DeleteWhenUnused(moduleAndSpirv.first);
    }
}

std::optional<ShaderModule::ModuleAndSpirv>
ShaderModule::ConcurrentTransformedShaderModuleCache::Find(
    const PipelineLayoutEntryPointPair& key) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mTransformedShaderModuleCache.find(key);
    if (iter != mTransformedShaderModuleCache.end()) {
        return std::make_pair(iter->second.first, iter->second.second.get());
    }
    return {};
}

ShaderModule::ModuleAndSpirv ShaderModule::ConcurrentTransformedShaderModuleCache::AddOrGet(
    const PipelineLayoutEntryPointPair& key,
    VkShaderModule module,
    Spirv&& spirv) {
    ASSERT(module != VK_NULL_HANDLE);
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mTransformedShaderModuleCache.find(key);
    if (iter == mTransformedShaderModuleCache.end()) {
        mTransformedShaderModuleCache.emplace(
            key, std::make_pair(module, std::unique_ptr<Spirv>(new Spirv(spirv))));
    } else {
        mDevice->GetFencedDeleter()->DeleteWhenUnused(module);
    }
    // Now the key should exist in the map, so find it again and return it.
    iter = mTransformedShaderModuleCache.find(key);
    return std::make_pair(iter->second.first, iter->second.second.get());
}

// static
ResultOrError<Ref<ShaderModule>> ShaderModule::Create(
    Device* device,
    const ShaderModuleDescriptor* descriptor,
    ShaderModuleParseResult* parseResult,
    OwnedCompilationMessages* compilationMessages) {
    Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor));
    DAWN_TRY(module->Initialize(parseResult, compilationMessages));
    return module;
}

ShaderModule::ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor)
    : ShaderModuleBase(device, descriptor),
      mTransformedShaderModuleCache(
          std::make_unique<ConcurrentTransformedShaderModuleCache>(device)) {}

MaybeError ShaderModule::Initialize(ShaderModuleParseResult* parseResult,
                                    OwnedCompilationMessages* compilationMessages) {
    if (GetDevice()->IsRobustnessEnabled()) {
        ScopedTintICEHandler scopedICEHandler(GetDevice());

        tint::transform::Robustness robustness;
        tint::transform::DataMap transformInputs;

        tint::Program program;
        DAWN_TRY_ASSIGN(program, RunTransforms(&robustness, parseResult->tintProgram.get(),
                                               transformInputs, nullptr, nullptr));
        // Rather than use a new ParseResult object, we just reuse the original parseResult
        parseResult->tintProgram = std::make_unique<tint::Program>(std::move(program));
    }

    return InitializeBase(parseResult, compilationMessages);
}

void ShaderModule::DestroyImpl() {
    ShaderModuleBase::DestroyImpl();
    // Remove reference to internal cache to trigger cleanup.
    mTransformedShaderModuleCache = nullptr;
}

ShaderModule::~ShaderModule() = default;

#define SPIRV_COMPILATION_REQUEST_MEMBERS(X)                                    \
    X(const tint::Program*, inputProgram)                                       \
    X(tint::transform::BindingRemapper::BindingPoints, bindingPoints)           \
    X(tint::transform::MultiplanarExternalTexture::BindingsMap, newBindingsMap) \
    X(const char*, entryPointName)                                              \
    X(bool, disableWorkgroupInit)                                               \
    X(bool, useZeroInitializeWorkgroupMemoryExtension)                          \
    X(bool, dumpShaders)                                                        \
    X(CacheKey::UnsafeUnkeyedValue<dawn::platform::Platform*>, tracePlatform)   \
    X(LogSink, logSink)

DAWN_MAKE_CACHE_REQUEST_BUILDER(SpirvCompilationRequest, SPIRV_COMPILATION_REQUEST_MEMBERS);
#undef SPIRV_COMPILATION_REQUEST_MEMBERS

ResultOrError<ShaderModule::ModuleAndSpirv> ShaderModule::GetHandleAndSpirv(
    const char* entryPointName,
    const PipelineLayout* layout) {
    TRACE_EVENT0(GetDevice()->GetPlatform(), General, "ShaderModuleVk::GetHandleAndSpirv");

    // If the shader was destroyed, we should never call this function.
    ASSERT(IsAlive());

    ScopedTintICEHandler scopedICEHandler(GetDevice());

    // Check to see if we have the handle and spirv cached already.
    auto cacheKey = std::make_pair(layout, entryPointName);
    auto handleAndSpirv = mTransformedShaderModuleCache->Find(cacheKey);
    if (handleAndSpirv.has_value()) {
        return std::move(*handleAndSpirv);
    }

    // Creation of module and spirv is deferred to this point when using tint generator

    // Remap BindingNumber to BindingIndex in WGSL shader
    using BindingRemapper = tint::transform::BindingRemapper;
    using BindingPoint = tint::transform::BindingPoint;
    BindingRemapper::BindingPoints bindingPoints;

    const BindingInfoArray& moduleBindingInfo = GetEntryPoint(entryPointName).bindings;

    for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
        const BindGroupLayout* bgl = ToBackend(layout->GetBindGroupLayout(group));
        const auto& groupBindingInfo = moduleBindingInfo[group];
        for (const auto& [binding, _] : groupBindingInfo) {
            BindingIndex bindingIndex = bgl->GetBindingIndex(binding);
            BindingPoint srcBindingPoint{static_cast<uint32_t>(group),
                                         static_cast<uint32_t>(binding)};

            BindingPoint dstBindingPoint{static_cast<uint32_t>(group),
                                         static_cast<uint32_t>(bindingIndex)};
            if (srcBindingPoint != dstBindingPoint) {
                bindingPoints.emplace(srcBindingPoint, dstBindingPoint);
            }
        }
    }

    // Transform external textures into the binding locations specified in the bgl
    // TODO(dawn:1082): Replace this block with ShaderModuleBase::AddExternalTextureTransform.
    tint::transform::MultiplanarExternalTexture::BindingsMap newBindingsMap;
    for (BindGroupIndex i : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
        const BindGroupLayoutBase* bgl = layout->GetBindGroupLayout(i);

        for (const auto& [_, expansion] : bgl->GetExternalTextureBindingExpansionMap()) {
            newBindingsMap[{static_cast<uint32_t>(i),
                            static_cast<uint32_t>(bgl->GetBindingIndex(expansion.plane0))}] = {
                {static_cast<uint32_t>(i),
                 static_cast<uint32_t>(bgl->GetBindingIndex(expansion.plane1))},
                {static_cast<uint32_t>(i),
                 static_cast<uint32_t>(bgl->GetBindingIndex(expansion.params))}};
        }
    }

    Spirv spirv;
#if TINT_BUILD_SPV_WRITER
    auto req = MakeSpirvCompilationRequest()
                   .inputProgram(GetTintProgram())
                   .bindingPoints(std::move(bindingPoints))
                   .newBindingsMap(std::move(newBindingsMap))
                   .entryPointName(entryPointName)
                   .disableWorkgroupInit(GetDevice()->IsToggleEnabled(Toggle::DisableWorkgroupInit))
                   .useZeroInitializeWorkgroupMemoryExtension(GetDevice()->IsToggleEnabled(
                       Toggle::VulkanUseZeroInitializeWorkgroupMemoryExtension))
                   .dumpShaders(GetDevice()->IsToggleEnabled(Toggle::DumpShaders))
                   .tracePlatform(UnsafeUnkeyedValue(GetDevice()->GetPlatform()))
                   .logSink(LogSink(GetDevice()));

    CacheKey key = req.CreateCacheKey();
    auto blob = GetDevice()->GetBlobCache()->Load(key);
    if (!blob.Empty()) {
        spirv = std::move(blob);
    } else {
        DAWN_TRY_ASSIGN(spirv, std::move(req).Call([](SpirvCompilationRequest r)
                                                       -> ResultOrError<Spirv> {
            tint::transform::Manager transformManager;
            // Many Vulkan drivers can't handle multi-entrypoint shader modules.
            transformManager.append(std::make_unique<tint::transform::SingleEntryPoint>());
            // Run the binding remapper after SingleEntryPoint to avoid collisions with
            // unused entryPoints.
            transformManager.append(std::make_unique<tint::transform::BindingRemapper>());
            tint::transform::DataMap transformInputs;
            transformInputs.Add<tint::transform::SingleEntryPoint::Config>(r.entryPointName);
            transformInputs.Add<BindingRemapper::Remappings>(std::move(r.bindingPoints),
                                                             BindingRemapper::AccessControls{},
                                                             /* mayCollide */ false);
            if (!r.newBindingsMap.empty()) {
                transformManager.Add<tint::transform::MultiplanarExternalTexture>();
                transformInputs.Add<tint::transform::MultiplanarExternalTexture::NewBindingPoints>(
                    r.newBindingsMap);
            }
            tint::Program program;
            {
                TRACE_EVENT0(r.tracePlatform.UnsafeGetValue(), General, "RunTransforms");
                DAWN_TRY_ASSIGN(program, RunTransforms(&transformManager, r.inputProgram,
                                                       transformInputs, nullptr, nullptr));
            }
            tint::writer::spirv::Options options;
            options.emit_vertex_point_size = true;
            options.disable_workgroup_init = r.disableWorkgroupInit;
            options.use_zero_initialize_workgroup_memory_extension =
                r.useZeroInitializeWorkgroupMemoryExtension;

            Spirv spirv;
            {
                TRACE_EVENT0(r.tracePlatform.UnsafeGetValue(), General,
                             "tint::writer::spirv::Generate()");
                auto result = tint::writer::spirv::Generate(&program, options);
                DAWN_INVALID_IF(!result.success, "An error occured while generating SPIR-V: %s.",
                                result.error);
                spirv = std::move(result.spirv);
            }
            DAWN_INVALID_IF(
                !ValidateSpirv(std::move(r.logSink), spirv.Data(), spirv.size(), r.dumpShaders),
                "Produced invalid SPIRV. Please file a bug at https://crbug.com/tint.");
            return spirv;
        }));

        GetDevice()->GetBlobCache()->Store(key, spirv.SizeInBytes(), spirv.Data());
    }
#else
    return DAWN_INTERNAL_ERROR("TINT_BUILD_SPV_WRITER is not defined.");
#endif

    VkShaderModuleCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = spirv.SizeInBytes();
    createInfo.pCode = spirv.Data();

    Device* device = ToBackend(GetDevice());

    VkShaderModule newHandle = VK_NULL_HANDLE;
    {
        TRACE_EVENT0(GetDevice()->GetPlatform(), General, "vkCreateShaderModule");
        DAWN_TRY(CheckVkSuccess(
            device->fn.CreateShaderModule(device->GetVkDevice(), &createInfo, nullptr, &*newHandle),
            "CreateShaderModule"));
    }
    ModuleAndSpirv moduleAndSpirv;
    if (newHandle != VK_NULL_HANDLE) {
        moduleAndSpirv =
            mTransformedShaderModuleCache->AddOrGet(cacheKey, newHandle, std::move(spirv));
    }

    SetDebugName(ToBackend(GetDevice()), moduleAndSpirv.first, "Dawn_ShaderModule", GetLabel());

    return std::move(moduleAndSpirv);
}

}  // namespace dawn::native::vulkan
