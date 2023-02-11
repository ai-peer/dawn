// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d11/AdapterD3D11.h"

#include <string>

#include "dawn/common/Constants.h"
#include "dawn/common/WindowsUtils.h"
#include "dawn/native/Instance.h"
#include "dawn/native/d3d11/BackendD3D11.h"
#include "dawn/native/d3d11/D3D11Error.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/PlatformFunctionsD3D11.h"
// #include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

Adapter::Adapter(Backend* backend,
                 ComPtr<IDXGIAdapter3> hardwareAdapter,
                 const TogglesState& adapterToggles)
    : AdapterBase(backend->GetInstance(), wgpu::BackendType::D3D11, adapterToggles),
      mHardwareAdapter(std::move(hardwareAdapter)),
      mBackend(backend) {}

Adapter::~Adapter() = default;

bool Adapter::SupportsExternalImages() const {
    // Via dawn::native::d3d11::ExternalImageDXGI::Create
    return true;
}

const D3D11DeviceInfo& Adapter::GetDeviceInfo() const {
    return mDeviceInfo;
}

IDXGIAdapter3* Adapter::GetHardwareAdapter() const {
    return mHardwareAdapter.Get();
}

Backend* Adapter::GetBackend() const {
    return mBackend;
}

ComPtr<ID3D11Device> Adapter::GetDevice() const {
    return mD3d11Device;
}

MaybeError Adapter::InitializeImpl() {
    // D3D11 cannot check for feature support without a device.
    // Create the device to populate the adapter properties then reuse it when needed for actual
    // rendering.
    const PlatformFunctions* functions = GetBackend()->GetFunctions();
    const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    UINT flags = 0;
    if (GetInstance()->IsBackendValidationEnabled() || true) {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    DAWN_TRY(CheckHRESULT(functions->d3d11CreateDevice(
                              GetHardwareAdapter(), D3D_DRIVER_TYPE_UNKNOWN, /*Software=*/nullptr,
                              flags, featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                              &mD3d11Device, &mFeatureLevel, /*[out] ppImmediateContext=*/nullptr),
                          "D3D11CreateDevice failed"));

    DXGI_ADAPTER_DESC1 adapterDesc;
    ComPtr<IDXGIAdapter1> adapter1;
    DAWN_TRY(CheckHRESULT(mHardwareAdapter.As(&adapter1), "IDXGIAdapter::QueryInterface"));
    adapter1->GetDesc1(&adapterDesc);

    mDeviceId = adapterDesc.DeviceId;
    mVendorId = adapterDesc.VendorId;
    mName = WCharToUTF8(adapterDesc.Description);

    DAWN_TRY_ASSIGN(mDeviceInfo, GatherDeviceInfo(*this));

    if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        mAdapterType = wgpu::AdapterType::CPU;
    } else {
        mAdapterType =
            (mDeviceInfo.isUMA) ? wgpu::AdapterType::IntegratedGPU : wgpu::AdapterType::DiscreteGPU;
    }

    // Convert the adapter's D3D11 driver version to a readable string like "24.21.13.9793".
    LARGE_INTEGER umdVersion;
    if (mHardwareAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion) !=
        DXGI_ERROR_UNSUPPORTED) {
        uint64_t encodedVersion = umdVersion.QuadPart;
        uint16_t mask = 0xFFFF;
        mDriverVersion = {static_cast<uint16_t>((encodedVersion >> 48) & mask),
                          static_cast<uint16_t>((encodedVersion >> 32) & mask),
                          static_cast<uint16_t>((encodedVersion >> 16) & mask),
                          static_cast<uint16_t>(encodedVersion & mask)};
        mDriverDescription = std::string("D3D11 driver version ") + mDriverVersion.ToString();
    }

    return {};
}

bool Adapter::AreTimestampQueriesSupported() const {
    // XXX:
    return false;
}

void Adapter::InitializeSupportedFeaturesImpl() {
    mSupportedFeatures.EnableFeature(Feature::TextureCompressionBC);
    mSupportedFeatures.EnableFeature(Feature::MultiPlanarFormats);
    mSupportedFeatures.EnableFeature(Feature::Depth32FloatStencil8);
    mSupportedFeatures.EnableFeature(Feature::IndirectFirstInstance);
    mSupportedFeatures.EnableFeature(Feature::RG11B10UfloatRenderable);
    mSupportedFeatures.EnableFeature(Feature::DepthClipControl);

    if (AreTimestampQueriesSupported()) {
        mSupportedFeatures.EnableFeature(Feature::TimestampQuery);
        mSupportedFeatures.EnableFeature(Feature::TimestampQueryInsidePasses);
    }
    mSupportedFeatures.EnableFeature(Feature::PipelineStatisticsQuery);

    // Both Dp4a and ShaderF16 features require DXC version being 1.4 or higher
    if (GetBackend()->IsDXCAvailableAndVersionAtLeast(1, 4, 1, 4)) {
        if (mDeviceInfo.supportsDP4a) {
            mSupportedFeatures.EnableFeature(Feature::ChromiumExperimentalDp4a);
        }
        if (mDeviceInfo.supportsShaderF16) {
            mSupportedFeatures.EnableFeature(Feature::ShaderF16);
        }
    }
}

MaybeError Adapter::InitializeSupportedLimitsImpl(CombinedLimits* limits) {
    GetDefaultLimits(&limits->v1);

    // // https://docs.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels

    // Limits that are the same across D3D feature levels
    limits->v1.maxTextureDimension1D = D3D11_REQ_TEXTURE1D_U_DIMENSION;
    limits->v1.maxTextureDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    limits->v1.maxTextureDimension3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    limits->v1.maxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    // Slot values can be 0-15, inclusive:
    // https://docs.microsoft.com/en-ca/windows/win32/api/d3d11/ns-d3d11-d3d11_input_element_desc
    limits->v1.maxVertexBuffers = 16;
    limits->v1.maxVertexAttributes = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;

    uint32_t maxUAVsAllStages = mFeatureLevel == D3D_FEATURE_LEVEL_11_1
                                    ? D3D11_1_UAV_SLOT_COUNT
                                    : D3D11_PS_CS_UAV_REGISTER_COUNT;
    ASSERT(maxUAVsAllStages / 4 > limits->v1.maxStorageTexturesPerShaderStage);
    ASSERT(maxUAVsAllStages / 4 > limits->v1.maxStorageBuffersPerShaderStage);
    uint32_t maxUAVsPerStage = maxUAVsAllStages / 2;

    limits->v1.maxUniformBuffersPerShaderStage = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    // Allocate half of the UAVs to storage buffers, and half to storage textures.
    limits->v1.maxStorageTexturesPerShaderStage = maxUAVsPerStage / 2;
    limits->v1.maxStorageBuffersPerShaderStage = maxUAVsPerStage - maxUAVsPerStage / 2;
    limits->v1.maxSampledTexturesPerShaderStage = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    limits->v1.maxSamplersPerShaderStage = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    limits->v1.maxColorAttachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;

    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits
    // In DWORDS. Descriptor tables cost 1, Root constants cost 1, Root descriptors cost 2.
    static constexpr uint32_t kMaxRootSignatureSize = 64u;
    // Dawn maps WebGPU's binding model by:
    //  - (maxBindGroups)
    //    CBVs/UAVs/SRVs for bind group are a root descriptor table
    //  - (maxBindGroups)
    //    Samplers for each bind group are a root descriptor table
    //  - (2 * maxDynamicBuffers)
    //    Each dynamic buffer is a root descriptor
    //  RESERVED:
    //  - 3 = max of:
    //    - 2 root constants for the baseVertex/baseInstance constants.
    //    - 3 root constants for num workgroups X, Y, Z
    //  - 4 root constants (kMaxDynamicStorageBuffersPerPipelineLayout) for dynamic storage
    //  buffer lengths.
    static constexpr uint32_t kReservedSlots = 7;

    // Available slots after base limits considered.
    uint32_t availableRootSignatureSlots =
        kMaxRootSignatureSize - kReservedSlots -
        2 * (limits->v1.maxBindGroups + limits->v1.maxDynamicUniformBuffersPerPipelineLayout +
             limits->v1.maxDynamicStorageBuffersPerPipelineLayout);

    // Because we need either:
    //  - 1 cbv/uav/srv table + 1 sampler table
    //  - 2 slots for a root descriptor
    uint32_t availableDynamicBufferOrBindGroup = availableRootSignatureSlots / 2;

    // We can either have a bind group, a dyn uniform buffer or a dyn storage buffer.
    // Distribute evenly.
    limits->v1.maxBindGroups += availableDynamicBufferOrBindGroup / 3;
    limits->v1.maxDynamicUniformBuffersPerPipelineLayout += availableDynamicBufferOrBindGroup / 3;
    limits->v1.maxDynamicStorageBuffersPerPipelineLayout +=
        (availableDynamicBufferOrBindGroup - 2 * (availableDynamicBufferOrBindGroup / 3));

    ASSERT(2 * (limits->v1.maxBindGroups + limits->v1.maxDynamicUniformBuffersPerPipelineLayout +
                limits->v1.maxDynamicStorageBuffersPerPipelineLayout) <=
           kMaxRootSignatureSize - kReservedSlots);

    // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads
    limits->v1.maxComputeWorkgroupSizeX = D3D11_CS_THREAD_GROUP_MAX_X;
    limits->v1.maxComputeWorkgroupSizeY = D3D11_CS_THREAD_GROUP_MAX_Y;
    limits->v1.maxComputeWorkgroupSizeZ = D3D11_CS_THREAD_GROUP_MAX_Z;
    limits->v1.maxComputeInvocationsPerWorkgroup = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;

    // https://docs.maxComputeWorkgroupSizeXmicrosoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_dispatch_arguments
    limits->v1.maxComputeWorkgroupsPerDimension = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;

    limits->v1.maxComputeWorkgroupStorageSize = 32768;

    // Max number of "constants" where each constant is a 16-byte float4
    limits->v1.maxUniformBufferBindingSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    // D3D11 has no documented limit on the size of a storage buffer binding.
    limits->v1.maxStorageBufferBindingSize = 4294967295;
    // D3D11 has no documented limit on the buffer size.
    limits->v1.maxBufferSize = kAssumedMaxBufferSize;

    return {};
}

MaybeError Adapter::ValidateFeatureSupportedWithDeviceTogglesImpl(
    wgpu::FeatureName feature,
    const TogglesState& deviceTogglesState) {
    // shader-f16 feature and chromium-experimental-dp4a feature require DXC 1.4 or higher for
    // D3D11.
    if (feature == wgpu::FeatureName::ShaderF16 ||
        feature == wgpu::FeatureName::ChromiumExperimentalDp4a) {
        DAWN_INVALID_IF(!(deviceTogglesState.IsEnabled(Toggle::UseDXC) &&
                          mBackend->IsDXCAvailableAndVersionAtLeast(1, 4, 1, 4)),
                        "Feature %s requires DXC for D3D11.",
                        GetInstance()->GetFeatureInfo(feature)->name);
    }
    return {};
}

void Adapter::SetupBackendDeviceToggles(TogglesState* deviceToggles) const {
    // const bool useResourceHeapTier2 = (GetDeviceInfo().resourceHeapTier >= 2);
    // deviceToggles->Default(Toggle::UseD3D11ResourceHeapTier2, useResourceHeapTier2);
    // deviceToggles->Default(Toggle::UseD3D11RenderPass, GetDeviceInfo().supportsRenderPass);
    // deviceToggles->Default(Toggle::UseD3D11ResidencyManagement, true);
    // deviceToggles->Default(Toggle::D3D11AlwaysUseTypelessFormatsForCastableTexture,
    //                        !GetDeviceInfo().supportsCastingFullyTypedFormat);
    // deviceToggles->Default(Toggle::ApplyClearBigIntegerColorValueWithDraw, true);

    // // The restriction on the source box specifying a portion of the depth stencil texture in
    // // CopyTextureRegion() is only available on the D3D11 platforms which doesn't support
    // // programmable sample positions.
    // deviceToggles->Default(
    //     Toggle::D3D11UseTempBufferInDepthStencilTextureAndBufferCopyWithNonZeroBufferOffset,
    //     GetDeviceInfo().programmableSamplePositionsTier == 0);

    // // Check DXC for use_dxc toggle, and default to use FXC
    // // TODO(dawn:1495): When implementing adapter toggles, promote UseDXC as adapter toggle, and
    // do
    // // the validation when creating adapters.
    // if (!GetBackend()->IsDXCAvailable()) {
    //     deviceToggles->ForceSet(Toggle::UseDXC, false);
    // }
    // deviceToggles->Default(Toggle::UseDXC, false);

    // // Disable optimizations when using FXC
    // // See https://crbug.com/dawn/1203
    // deviceToggles->Default(Toggle::FxcOptimizations, false);

    // // By default use the maximum shader-visible heap size allowed.
    // deviceToggles->Default(Toggle::UseD3D11SmallShaderVisibleHeapForTesting, false);

    // uint32_t deviceId = GetDeviceId();
    // uint32_t vendorId = GetVendorId();

    // // Currently this workaround is only needed on Intel Gen9, Gen9.5 and Gen11 GPUs.
    // // See http://crbug.com/1161355 for more information.
    // if (gpu_info::IsIntelGen9(vendorId, deviceId) || gpu_info::IsIntelGen11(vendorId, deviceId))
    // {
    //     const gpu_info::DriverVersion kFixedDriverVersion = {31, 0, 101, 2114};
    //     if (gpu_info::CompareWindowsDriverVersion(vendorId, GetDriverVersion(),
    //                                               kFixedDriverVersion) < 0) {
    //         deviceToggles->Default(
    //             Toggle::UseTempBufferInSmallFormatTextureToTextureCopyFromGreaterToLessMipLevel,
    //             true);
    //     }
    // }

    // // Currently this workaround is only needed on Intel Gen9, Gen9.5 and Gen12 GPUs.
    // // See http://crbug.com/dawn/1487 for more information.
    // if (gpu_info::IsIntelGen9(vendorId, deviceId) || gpu_info::IsIntelGen12LP(vendorId, deviceId)
    // ||
    //     gpu_info::IsIntelGen12HP(vendorId, deviceId)) {
    //     deviceToggles->Default(Toggle::D3D11ForceClearCopyableDepthStencilTextureOnCreation,
    //     true);
    // }

    // // Currently this workaround is only needed on Intel Gen12 GPUs.
    // // See http://crbug.com/dawn/1487 for more information.
    // if (gpu_info::IsIntelGen12LP(vendorId, deviceId) ||
    //     gpu_info::IsIntelGen12HP(vendorId, deviceId)) {
    //     deviceToggles->Default(Toggle::D3D11DontSetClearValueOnDepthTextureCreation, true);
    // }

    // // Currently this workaround is needed on any D3D11 backend for some particular situations.
    // // But we may need to limit it if D3D11 runtime fixes the bug on its new release. See
    // // https://crbug.com/dawn/1289 for more information.
    // // TODO(dawn:1289): Unset this toggle when we skip the split on the buffer-texture copy
    // // on the platforms where UnrestrictedBufferTextureCopyPitchSupported is true.
    // deviceToggles->Default(Toggle::D3D11SplitBufferTextureCopyForRowsPerImagePaddings, true);

    // // This workaround is only needed on Intel Gen12LP with driver prior to 30.0.101.1692.
    // // See http://crbug.com/dawn/949 for more information.
    // if (gpu_info::IsIntelGen12LP(vendorId, deviceId)) {
    //     const gpu_info::DriverVersion kFixedDriverVersion = {30, 0, 101, 1692};
    //     if (gpu_info::CompareWindowsDriverVersion(vendorId, GetDriverVersion(),
    //                                               kFixedDriverVersion) == -1) {
    //         deviceToggles->Default(Toggle::D3D11AllocateExtraMemoryFor2DArrayColorTexture, true);
    //     }
    // }

    // // Currently these workarounds are only needed on Intel Gen9.5 and Gen11 GPUs.
    // // See http://crbug.com/1237175 and http://crbug.com/dawn/1628 for more information.
    // if ((gpu_info::IsIntelGen9(vendorId, deviceId) && !gpu_info::IsSkylake(deviceId)) ||
    //     gpu_info::IsIntelGen11(vendorId, deviceId)) {
    //     deviceToggles->Default(
    //         Toggle::D3D11Allocate2DTextureWithCopyDstOrRenderAttachmentAsCommittedResource,
    //         true);
    //     // Now we don't need to force clearing depth stencil textures with CopyDst as all the
    //     depth
    //     // stencil textures (can only be 2D textures) will be created with
    //     CreateCommittedResource()
    //     // instead of CreatePlacedResource().
    //     deviceToggles->Default(Toggle::D3D11ForceClearCopyableDepthStencilTextureOnCreation,
    //     false);
    // }

    // // Currently this toggle is only needed on Intel Gen9 and Gen9.5 GPUs.
    // // See http://crbug.com/dawn/1579 for more information.
    // if (gpu_info::IsIntelGen9(vendorId, deviceId)) {
    //     deviceToggles->ForceSet(Toggle::NoWorkaroundDstAlphaBlendDoesNotWork, true);
    // }
}

ResultOrError<Ref<DeviceBase>> Adapter::CreateDeviceImpl(const DeviceDescriptor* descriptor,
                                                         const TogglesState& deviceToggles) {
    return Device::Create(this, descriptor, deviceToggles);
}

// Resets the backend device and creates a new one. If any D3D11 objects belonging to the
// current ID3D11Device have not been destroyed, a non-zero value will be returned upon Reset()
// and the subequent call to CreateDevice will return a handle the existing device instead of
// creating a new one.
MaybeError Adapter::ResetInternalDeviceForTestingImpl() {
    ASSERT(mD3d11Device.Reset() == 0);
    DAWN_TRY(Initialize());

    return {};
}

}  // namespace dawn::native::d3d11
