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

#ifndef SRC_DAWN_NATIVE_TOGGLES_H_
#define SRC_DAWN_NATIVE_TOGGLES_H_

#include <bitset>
#include <string>
#include <unordered_map>
#include <vector>

#include "dawn/native/DawnNative.h"

namespace dawn::native {

struct DawnTogglesDescriptor;

enum class Toggle {
    EmulateStoreAndMSAAResolve,
    NonzeroClearResourcesOnCreationForTesting,
    AlwaysResolveIntoZeroLevelAndLayer,
    LazyClearResourceOnFirstUse,
    TurnOffVsync,
    UseTemporaryBufferInCompressedTextureToTextureCopy,
    UseD3D12ResourceHeapTier2,
    UseD3D12RenderPass,
    UseD3D12ResidencyManagement,
    DisableResourceSuballocation,
    SkipValidation,
    VulkanUseD32S8,
    VulkanUseS8,
    MetalDisableSamplerCompare,
    MetalUseSharedModeForCounterSampleBuffer,
    DisableBaseVertex,
    DisableBaseInstance,
    DisableIndexedDrawBuffers,
    DisableSnormRead,
    DisableDepthRead,
    DisableStencilRead,
    DisableDepthStencilRead,
    DisableBGRARead,
    DisableSampleVariables,
    UseD3D12SmallShaderVisibleHeapForTesting,
    UseDXC,
    DisableRobustness,
    MetalEnableVertexPulling,
    DisallowUnsafeAPIs,
    FlushBeforeClientWaitSync,
    UseTempBufferInSmallFormatTextureToTextureCopyFromGreaterToLessMipLevel,
    EmitHLSLDebugSymbols,
    DisallowSpirv,
    DumpShaders,
    ForceWGSLStep,
    DisableWorkgroupInit,
    DisableSymbolRenaming,
    UseUserDefinedLabelsInBackend,
    UsePlaceholderFragmentInVertexOnlyPipeline,
    FxcOptimizations,
    RecordDetailedTimingInTraceEvents,
    DisableTimestampQueryConversion,
    VulkanUseZeroInitializeWorkgroupMemoryExtension,
    D3D12SplitBufferTextureCopyForRowsPerImagePaddings,
    MetalRenderR8RG8UnormSmallMipToTempTexture,
    DisableBlobCache,
    D3D12ForceClearCopyableDepthStencilTextureOnCreation,
    D3D12DontSetClearValueOnDepthTextureCreation,
    D3D12AlwaysUseTypelessFormatsForCastableTexture,
    D3D12AllocateExtraMemoryFor2DArrayColorTexture,
    D3D12UseTempBufferInDepthStencilTextureAndBufferCopyWithNonZeroBufferOffset,
    ApplyClearBigIntegerColorValueWithDraw,
    MetalUseMockBlitEncoderForWriteTimestamp,
    VulkanSplitCommandBufferOnDepthStencilComputeSampleAfterRenderPass,
    D3D12Allocate2DTexturewithCopyDstAsCommittedResource,
    DisallowDeprecatedAPIs,

    // Unresolved issues.
    NoWorkaroundSampleMaskBecomesZeroForAllButLastColorTarget,
    NoWorkaroundIndirectBaseVertexNotApplied,
    NoWorkaroundDstAlphaBlendDoesNotWork,

    EnumCount,
    InvalidEnum = EnumCount,
};

typedef std::bitset<static_cast<size_t>(Toggle::EnumCount)> TogglesBitSet;

// A wrapper of the bitset to store if a toggle is present or not. This wrapper provides the
// convenience to convert the enums of enum class Toggle to the indices of a bitset.
struct TogglesSet {
    TogglesBitSet toggleBitset;

    void Set(Toggle toggle, bool enabled);
    bool Has(Toggle toggle) const;
    std::vector<const char*> GetContainedToggleNames() const;

    bool operator==(const TogglesSet& rhs) const;
    bool operator!=(const TogglesSet& rhs) const;
};

// RequiredTogglesSet track each toggle with three posible states, i.e. "Not provided" (default),
// "Provided as enabled", and "Provided as disabled". This struct can be used to record the
// user-provided toggles, where some toggles are explicitly enabled or disabled while the other
// toggles are left as default.
struct RequiredTogglesSet {
    // Indicating what stage this RequiredTogglesSet object would be used. All set toggles in
    // RequiredTogglesSet must be of this stage.
    ToggleInfo::ToggleStage requiredStage;

    TogglesSet togglesIsProvided;
    TogglesSet providedTogglesEnabled;

    // Delete the default constructor to ensure that the stage of required toggles state is
    // explicitly assigned.
    RequiredTogglesSet() = delete;
    explicit RequiredTogglesSet(ToggleInfo::ToggleStage stage) : requiredStage(stage) {}

    bool operator==(const RequiredTogglesSet& rhs) const;
    bool operator!=(const RequiredTogglesSet& rhs) const;

    // Create a RequiredTogglesSet from a DawnTogglesDescriptor, only considering toggles of
    // required toggle stage.
    static RequiredTogglesSet CreateFromTogglesDescriptor(const DawnTogglesDescriptor* togglesDesc,
                                                          ToggleInfo::ToggleStage requiredStage);
    // Provide a single toggle with given state.
    // void Set(Toggle toggle, bool enabled);
    bool IsRequired(Toggle toggle) const;
    // Return true if the toggle is provided in enable list, and false otherwise.
    bool IsEnabled(Toggle toggle) const;
    // Return true if the toggle is provided in disable list, and false otherwise.
    bool IsDisabled(Toggle toggle) const;
    std::vector<const char*> GetEnabledToggleNames() const;
    std::vector<const char*> GetDisabledToggleNames() const;
};

// Hasher for RequiredTogglesSet
struct RequiredTogglesSetHasher {
    std::size_t operator()(RequiredTogglesSet const& requiredTogglesSet) const noexcept {
        std::size_t h1 =
            std::hash<TogglesBitSet>{}(requiredTogglesSet.togglesIsProvided.toggleBitset);
        std::size_t h2 =
            std::hash<TogglesBitSet>{}(requiredTogglesSet.providedTogglesEnabled.toggleBitset);
        return h1 ^ (h2 << 1);
    }
};

// TogglesState hold the actual state of toggles for instances, adapters and devices. It also keep a
// record of the required toggles set when creating the instance/adapter/device.
class TogglesState {
  public:
    // Delete the default constructor to ensure that the stage of toggles state is explicitly
    // assigned.
    TogglesState() = delete;
    // Create an empty toggles state of given stage
    explicit TogglesState(ToggleInfo::ToggleStage stage)
        : togglesStateStage(stage), requiredTogglesSet(stage) {}

    static TogglesState CreateFromRequiredTogglesSet(const RequiredTogglesSet& requiredTogglesSet);

    static TogglesState CreateFromRequiredAndInheritedToggles(
        const RequiredTogglesSet& requiredTogglesSet,
        const TogglesState& inheritedToggles);

    // Create a toggles state of given stage and given toggles enabled/disabled for testing. Note
    // that this toggles state may break the inheritance and force set policy, and has an empty
    // required toggles set rocord.
    static TogglesState CreateFromInitializerForTesting(
        ToggleInfo::ToggleStage togglesStateStage,
        std::initializer_list<Toggle> enabledToggles,
        std::initializer_list<Toggle> disabledToggles);

    // Set a toggle of the same stage of toggles state stage if and only if it is not already set.
    void Default(Toggle toggle, bool enabled);
    // Set a toggle of a stage earlier than toggles state stage.
    void Inherit(Toggle toggle, bool enabled);
    // Force set a toggle of a stage equals to or earlier than the toggles state stage. A force-set
    // toggle will get inherited to all later stage as forced.
    void ForceSet(Toggle toggle, bool enabled);

    // Return whether the toggle is set or not. Force-set is always treated as set.
    bool IsSet(Toggle toggle) const;
    // Return whether the toggle is force set or not.
    bool IsForced(Toggle toggle) const;
    // Return true if and only if the toggle is set to true.
    bool IsEnabled(Toggle toggle) const;
    // Return true if and only if the toggle is set to false.
    bool IsDisabled(Toggle toggle) const;
    ToggleInfo::ToggleStage GetStage() const;
    std::vector<const char*> GetEnabledToggleNames() const;
    std::vector<const char*> GetDisabledToggleNames() const;

    const TogglesBitSet& GetEnabledBitSet() const;
    const TogglesBitSet& GetSetBitSet() const;
    const RequiredTogglesSet& GetRequiredTogglesSet() const;

  protected:
    ToggleInfo::ToggleStage togglesStateStage;
    TogglesSet togglesIsSet;
    TogglesSet togglesIsEnabled;
    TogglesSet togglesIsForced;
    RequiredTogglesSet requiredTogglesSet;
};

const char* ToggleEnumToName(Toggle toggle);

class TogglesInfo {
  public:
    TogglesInfo();
    ~TogglesInfo();

    // Used to query the details of a toggle. Return nullptr if toggleName is not a valid name
    // of a toggle supported in Dawn.
    const ToggleInfo* GetToggleInfo(const char* toggleName);
    // Used to query the details of a toggle enum, asserting toggle is not Toggle::InvalidEnum.
    static const ToggleInfo* GetToggleInfo(Toggle toggle);
    Toggle ToggleNameToEnum(const char* toggleName);

  private:
    void EnsureToggleNameToEnumMapInitialized();

    bool mToggleNameToEnumMapInitialized = false;
    std::unordered_map<std::string, Toggle> mToggleNameToEnumMap;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TOGGLES_H_
