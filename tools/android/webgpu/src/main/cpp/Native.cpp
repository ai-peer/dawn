#include <jni.h>
#include "gen/include/dawn/webgpu.h"

#define DEFAULT __attribute__((visibility("default")))

const char *getString(JNIEnv *env, const jobject pJobject) {
    if (!pJobject) {
        return nullptr;
    }
    return env->GetStringUTFChars(static_cast<jstring>(pJobject), 0);
}

struct UserData {
    JNIEnv *env;
    jobject callback;
};

const WGPUAdapterProperties convertAdapterProperties(JNIEnv *env, jobject obj);
const WGPUAdapterPropertiesD3D convertAdapterPropertiesD3D(JNIEnv *env, jobject obj);
const WGPUAdapterPropertiesVk convertAdapterPropertiesVk(JNIEnv *env, jobject obj);
const WGPUBindGroupEntry convertBindGroupEntry(JNIEnv *env, jobject obj);
const WGPUBlendComponent convertBlendComponent(JNIEnv *env, jobject obj);
const WGPUBufferBindingLayout convertBufferBindingLayout(JNIEnv *env, jobject obj);
const WGPUBufferDescriptor convertBufferDescriptor(JNIEnv *env, jobject obj);
const WGPUBufferHostMappedPointer convertBufferHostMappedPointer(JNIEnv *env, jobject obj);
const WGPUBufferMapCallbackInfo convertBufferMapCallbackInfo(JNIEnv *env, jobject obj);
const WGPUColor convertColor(JNIEnv *env, jobject obj);
const WGPUCommandBufferDescriptor convertCommandBufferDescriptor(JNIEnv *env, jobject obj);
const WGPUCommandEncoderDescriptor convertCommandEncoderDescriptor(JNIEnv *env, jobject obj);
const WGPUCompilationInfoCallbackInfo convertCompilationInfoCallbackInfo(JNIEnv *env, jobject obj);
const WGPUCompilationMessage convertCompilationMessage(JNIEnv *env, jobject obj);
const WGPUComputePassTimestampWrites convertComputePassTimestampWrites(JNIEnv *env, jobject obj);
const WGPUConstantEntry convertConstantEntry(JNIEnv *env, jobject obj);
const WGPUCopyTextureForBrowserOptions convertCopyTextureForBrowserOptions(JNIEnv *env, jobject obj);
const WGPUCreateComputePipelineAsyncCallbackInfo convertCreateComputePipelineAsyncCallbackInfo(JNIEnv *env, jobject obj);
const WGPUCreateRenderPipelineAsyncCallbackInfo convertCreateRenderPipelineAsyncCallbackInfo(JNIEnv *env, jobject obj);
const WGPUDawnAdapterPropertiesPowerPreference convertDawnAdapterPropertiesPowerPreference(JNIEnv *env, jobject obj);
const WGPUDawnBufferDescriptorErrorInfoFromWireClient convertDawnBufferDescriptorErrorInfoFromWireClient(JNIEnv *env, jobject obj);
const WGPUDawnCacheDeviceDescriptor convertDawnCacheDeviceDescriptor(JNIEnv *env, jobject obj);
const WGPUDawnComputePipelineFullSubgroups convertDawnComputePipelineFullSubgroups(JNIEnv *env, jobject obj);
const WGPUDawnEncoderInternalUsageDescriptor convertDawnEncoderInternalUsageDescriptor(JNIEnv *env, jobject obj);
const WGPUDawnExperimentalSubgroupLimits convertDawnExperimentalSubgroupLimits(JNIEnv *env, jobject obj);
const WGPUDawnMultisampleStateRenderToSingleSampled convertDawnMultisampleStateRenderToSingleSampled(JNIEnv *env, jobject obj);
const WGPUDawnRenderPassColorAttachmentRenderToSingleSampled convertDawnRenderPassColorAttachmentRenderToSingleSampled(JNIEnv *env, jobject obj);
const WGPUDawnShaderModuleSPIRVOptionsDescriptor convertDawnShaderModuleSPIRVOptionsDescriptor(JNIEnv *env, jobject obj);
const WGPUDawnTextureInternalUsageDescriptor convertDawnTextureInternalUsageDescriptor(JNIEnv *env, jobject obj);
const WGPUDawnWireWGSLControl convertDawnWireWGSLControl(JNIEnv *env, jobject obj);
const WGPUDepthStencilStateDepthWriteDefinedDawn convertDepthStencilStateDepthWriteDefinedDawn(JNIEnv *env, jobject obj);
const WGPUDrmFormatProperties convertDrmFormatProperties(JNIEnv *env, jobject obj);
const WGPUExtent2D convertExtent2D(JNIEnv *env, jobject obj);
const WGPUExtent3D convertExtent3D(JNIEnv *env, jobject obj);
const WGPUExternalTextureBindingEntry convertExternalTextureBindingEntry(JNIEnv *env, jobject obj);
const WGPUExternalTextureBindingLayout convertExternalTextureBindingLayout(JNIEnv *env, jobject obj);
const WGPUFormatCapabilities convertFormatCapabilities(JNIEnv *env, jobject obj);
const WGPUFuture convertFuture(JNIEnv *env, jobject obj);
const WGPUInstanceFeatures convertInstanceFeatures(JNIEnv *env, jobject obj);
const WGPULimits convertLimits(JNIEnv *env, jobject obj);
const WGPUMemoryHeapInfo convertMemoryHeapInfo(JNIEnv *env, jobject obj);
const WGPUMultisampleState convertMultisampleState(JNIEnv *env, jobject obj);
const WGPUOrigin2D convertOrigin2D(JNIEnv *env, jobject obj);
const WGPUOrigin3D convertOrigin3D(JNIEnv *env, jobject obj);
const WGPUPipelineLayoutDescriptor convertPipelineLayoutDescriptor(JNIEnv *env, jobject obj);
const WGPUPipelineLayoutStorageAttachment convertPipelineLayoutStorageAttachment(JNIEnv *env, jobject obj);
const WGPUPopErrorScopeCallbackInfo convertPopErrorScopeCallbackInfo(JNIEnv *env, jobject obj);
const WGPUPrimitiveDepthClipControl convertPrimitiveDepthClipControl(JNIEnv *env, jobject obj);
const WGPUPrimitiveState convertPrimitiveState(JNIEnv *env, jobject obj);
const WGPUQuerySetDescriptor convertQuerySetDescriptor(JNIEnv *env, jobject obj);
const WGPUQueueDescriptor convertQueueDescriptor(JNIEnv *env, jobject obj);
const WGPUQueueWorkDoneCallbackInfo convertQueueWorkDoneCallbackInfo(JNIEnv *env, jobject obj);
const WGPURenderBundleDescriptor convertRenderBundleDescriptor(JNIEnv *env, jobject obj);
const WGPURenderBundleEncoderDescriptor convertRenderBundleEncoderDescriptor(JNIEnv *env, jobject obj);
const WGPURenderPassDepthStencilAttachment convertRenderPassDepthStencilAttachment(JNIEnv *env, jobject obj);
const WGPURenderPassDescriptorMaxDrawCount convertRenderPassDescriptorMaxDrawCount(JNIEnv *env, jobject obj);
const WGPURenderPassTimestampWrites convertRenderPassTimestampWrites(JNIEnv *env, jobject obj);
const WGPURequestAdapterCallbackInfo convertRequestAdapterCallbackInfo(JNIEnv *env, jobject obj);
const WGPURequestAdapterOptions convertRequestAdapterOptions(JNIEnv *env, jobject obj);
const WGPURequestDeviceCallbackInfo convertRequestDeviceCallbackInfo(JNIEnv *env, jobject obj);
const WGPUSamplerBindingLayout convertSamplerBindingLayout(JNIEnv *env, jobject obj);
const WGPUSamplerDescriptor convertSamplerDescriptor(JNIEnv *env, jobject obj);
const WGPUShaderModuleSPIRVDescriptor convertShaderModuleSPIRVDescriptor(JNIEnv *env, jobject obj);
const WGPUShaderModuleWGSLDescriptor convertShaderModuleWGSLDescriptor(JNIEnv *env, jobject obj);
const WGPUShaderModuleDescriptor convertShaderModuleDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedBufferMemoryBeginAccessDescriptor convertSharedBufferMemoryBeginAccessDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedBufferMemoryDescriptor convertSharedBufferMemoryDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedBufferMemoryEndAccessState convertSharedBufferMemoryEndAccessState(JNIEnv *env, jobject obj);
const WGPUSharedBufferMemoryProperties convertSharedBufferMemoryProperties(JNIEnv *env, jobject obj);
const WGPUSharedFenceDXGISharedHandleDescriptor convertSharedFenceDXGISharedHandleDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceDXGISharedHandleExportInfo convertSharedFenceDXGISharedHandleExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedFenceMTLSharedEventDescriptor convertSharedFenceMTLSharedEventDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceMTLSharedEventExportInfo convertSharedFenceMTLSharedEventExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedFenceDescriptor convertSharedFenceDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceExportInfo convertSharedFenceExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor convertSharedFenceVkSemaphoreOpaqueFDDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo convertSharedFenceVkSemaphoreOpaqueFDExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreSyncFDDescriptor convertSharedFenceVkSemaphoreSyncFDDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreSyncFDExportInfo convertSharedFenceVkSemaphoreSyncFDExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreZirconHandleDescriptor convertSharedFenceVkSemaphoreZirconHandleDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedFenceVkSemaphoreZirconHandleExportInfo convertSharedFenceVkSemaphoreZirconHandleExportInfo(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryDXGISharedHandleDescriptor convertSharedTextureMemoryDXGISharedHandleDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryEGLImageDescriptor convertSharedTextureMemoryEGLImageDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryIOSurfaceDescriptor convertSharedTextureMemoryIOSurfaceDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryAHardwareBufferDescriptor convertSharedTextureMemoryAHardwareBufferDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryBeginAccessDescriptor convertSharedTextureMemoryBeginAccessDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryDescriptor convertSharedTextureMemoryDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryDmaBufPlane convertSharedTextureMemoryDmaBufPlane(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryEndAccessState convertSharedTextureMemoryEndAccessState(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryOpaqueFDDescriptor convertSharedTextureMemoryOpaqueFDDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor convertSharedTextureMemoryVkDedicatedAllocationDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryVkImageLayoutBeginState convertSharedTextureMemoryVkImageLayoutBeginState(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryVkImageLayoutEndState convertSharedTextureMemoryVkImageLayoutEndState(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryZirconHandleDescriptor convertSharedTextureMemoryZirconHandleDescriptor(JNIEnv *env, jobject obj);
const WGPUStaticSamplerBindingLayout convertStaticSamplerBindingLayout(JNIEnv *env, jobject obj);
const WGPUStencilFaceState convertStencilFaceState(JNIEnv *env, jobject obj);
const WGPUStorageTextureBindingLayout convertStorageTextureBindingLayout(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptor convertSurfaceDescriptor(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromAndroidNativeWindow convertSurfaceDescriptorFromAndroidNativeWindow(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromCanvasHTMLSelector convertSurfaceDescriptorFromCanvasHTMLSelector(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromMetalLayer convertSurfaceDescriptorFromMetalLayer(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromWaylandSurface convertSurfaceDescriptorFromWaylandSurface(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromWindowsHWND convertSurfaceDescriptorFromWindowsHWND(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromWindowsCoreWindow convertSurfaceDescriptorFromWindowsCoreWindow(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromWindowsSwapChainPanel convertSurfaceDescriptorFromWindowsSwapChainPanel(JNIEnv *env, jobject obj);
const WGPUSurfaceDescriptorFromXlibWindow convertSurfaceDescriptorFromXlibWindow(JNIEnv *env, jobject obj);
const WGPUSwapChainDescriptor convertSwapChainDescriptor(JNIEnv *env, jobject obj);
const WGPUTextureBindingLayout convertTextureBindingLayout(JNIEnv *env, jobject obj);
const WGPUTextureBindingViewDimensionDescriptor convertTextureBindingViewDimensionDescriptor(JNIEnv *env, jobject obj);
const WGPUTextureDataLayout convertTextureDataLayout(JNIEnv *env, jobject obj);
const WGPUTextureViewDescriptor convertTextureViewDescriptor(JNIEnv *env, jobject obj);
const WGPUVertexAttribute convertVertexAttribute(JNIEnv *env, jobject obj);
const WGPUAdapterPropertiesMemoryHeaps convertAdapterPropertiesMemoryHeaps(JNIEnv *env, jobject obj);
const WGPUBindGroupDescriptor convertBindGroupDescriptor(JNIEnv *env, jobject obj);
const WGPUBindGroupLayoutEntry convertBindGroupLayoutEntry(JNIEnv *env, jobject obj);
const WGPUBlendState convertBlendState(JNIEnv *env, jobject obj);
const WGPUCompilationInfo convertCompilationInfo(JNIEnv *env, jobject obj);
const WGPUComputePassDescriptor convertComputePassDescriptor(JNIEnv *env, jobject obj);
const WGPUDepthStencilState convertDepthStencilState(JNIEnv *env, jobject obj);
const WGPUDrmFormatCapabilities convertDrmFormatCapabilities(JNIEnv *env, jobject obj);
const WGPUExternalTextureDescriptor convertExternalTextureDescriptor(JNIEnv *env, jobject obj);
const WGPUFutureWaitInfo convertFutureWaitInfo(JNIEnv *env, jobject obj);
const WGPUImageCopyBuffer convertImageCopyBuffer(JNIEnv *env, jobject obj);
const WGPUImageCopyExternalTexture convertImageCopyExternalTexture(JNIEnv *env, jobject obj);
const WGPUImageCopyTexture convertImageCopyTexture(JNIEnv *env, jobject obj);
const WGPUInstanceDescriptor convertInstanceDescriptor(JNIEnv *env, jobject obj);
const WGPUPipelineLayoutPixelLocalStorage convertPipelineLayoutPixelLocalStorage(JNIEnv *env, jobject obj);
const WGPUProgrammableStageDescriptor convertProgrammableStageDescriptor(JNIEnv *env, jobject obj);
const WGPURenderPassColorAttachment convertRenderPassColorAttachment(JNIEnv *env, jobject obj);
const WGPURenderPassStorageAttachment convertRenderPassStorageAttachment(JNIEnv *env, jobject obj);
const WGPURequiredLimits convertRequiredLimits(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryDmaBufDescriptor convertSharedTextureMemoryDmaBufDescriptor(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryProperties convertSharedTextureMemoryProperties(JNIEnv *env, jobject obj);
const WGPUSharedTextureMemoryVkImageDescriptor convertSharedTextureMemoryVkImageDescriptor(JNIEnv *env, jobject obj);
const WGPUSupportedLimits convertSupportedLimits(JNIEnv *env, jobject obj);
const WGPUTextureDescriptor convertTextureDescriptor(JNIEnv *env, jobject obj);
const WGPUVertexBufferLayout convertVertexBufferLayout(JNIEnv *env, jobject obj);
const WGPUBindGroupLayoutDescriptor convertBindGroupLayoutDescriptor(JNIEnv *env, jobject obj);
const WGPUColorTargetState convertColorTargetState(JNIEnv *env, jobject obj);
const WGPUComputePipelineDescriptor convertComputePipelineDescriptor(JNIEnv *env, jobject obj);
const WGPUDeviceDescriptor convertDeviceDescriptor(JNIEnv *env, jobject obj);
const WGPURenderPassDescriptor convertRenderPassDescriptor(JNIEnv *env, jobject obj);
const WGPURenderPassPixelLocalStorage convertRenderPassPixelLocalStorage(JNIEnv *env, jobject obj);
const WGPUVertexState convertVertexState(JNIEnv *env, jobject obj);
const WGPUFragmentState convertFragmentState(JNIEnv *env, jobject obj);
const WGPURenderPipelineDescriptor convertRenderPipelineDescriptor(JNIEnv *env, jobject obj);

const WGPUAdapterProperties convertAdapterProperties(JNIEnv *env, jobject obj) {
    jclass adapterPropertiesClass = env->FindClass("android/dawn/AdapterProperties");

    WGPUAdapterProperties converted = {
        .vendorID = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesClass, "getVendorID", "()I"))),
        .vendorName = getString(env, env->CallObjectMethod(obj, env->GetMethodID(adapterPropertiesClass, "getVendorName", "()Ljava/lang/String;"))),
        .architecture = getString(env, env->CallObjectMethod(obj, env->GetMethodID(adapterPropertiesClass, "getArchitecture", "()Ljava/lang/String;"))),
        .deviceID = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesClass, "getDeviceID", "()I"))),
        .name = getString(env, env->CallObjectMethod(obj, env->GetMethodID(adapterPropertiesClass, "getName", "()Ljava/lang/String;"))),
        .driverDescription = getString(env, env->CallObjectMethod(obj, env->GetMethodID(adapterPropertiesClass, "getDriverDescription", "()Ljava/lang/String;"))),
        .adapterType = static_cast<WGPUAdapterType>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesClass, "getAdapterType", "()I"))),
        .backendType = static_cast<WGPUBackendType>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesClass, "getBackendType", "()I"))),
        .compatibilityMode = env->CallBooleanMethod(obj, env->GetMethodID(adapterPropertiesClass, "getCompatibilityMode", "()Z"))
    };

    jclass AdapterPropertiesD3DClass = env->FindClass("android/dawn/AdapterPropertiesD3D");
    if (env->IsInstanceOf(obj, AdapterPropertiesD3DClass)) {
         converted.nextInChain = &(new WGPUAdapterPropertiesD3D(convertAdapterPropertiesD3D(env, obj)))->chain;
    }
    jclass AdapterPropertiesVkClass = env->FindClass("android/dawn/AdapterPropertiesVk");
    if (env->IsInstanceOf(obj, AdapterPropertiesVkClass)) {
         converted.nextInChain = &(new WGPUAdapterPropertiesVk(convertAdapterPropertiesVk(env, obj)))->chain;
    }
    jclass DawnAdapterPropertiesPowerPreferenceClass = env->FindClass("android/dawn/DawnAdapterPropertiesPowerPreference");
    if (env->IsInstanceOf(obj, DawnAdapterPropertiesPowerPreferenceClass)) {
         converted.nextInChain = &(new WGPUDawnAdapterPropertiesPowerPreference(convertDawnAdapterPropertiesPowerPreference(env, obj)))->chain;
    }
    jclass AdapterPropertiesMemoryHeapsClass = env->FindClass("android/dawn/AdapterPropertiesMemoryHeaps");
    if (env->IsInstanceOf(obj, AdapterPropertiesMemoryHeapsClass)) {
         converted.nextInChain = &(new WGPUAdapterPropertiesMemoryHeaps(convertAdapterPropertiesMemoryHeaps(env, obj)))->chain;
    }
    return converted;
}

const WGPUAdapterProperties* convertAdapterPropertiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUAdapterProperties(convertAdapterProperties(env, obj));
}

const WGPUAdapterProperties* convertAdapterPropertiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUAdapterProperties* entries = new WGPUAdapterProperties[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertAdapterProperties(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUAdapterPropertiesD3D convertAdapterPropertiesD3D(JNIEnv *env, jobject obj) {
    jclass adapterPropertiesD3DClass = env->FindClass("android/dawn/AdapterPropertiesD3D");

    WGPUAdapterPropertiesD3D converted = {
        .chain = {.sType = WGPUSType_AdapterPropertiesD3D},
        .shaderModel = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesD3DClass, "getShaderModel", "()I")))
    };

    return converted;
}

const WGPUAdapterPropertiesD3D* convertAdapterPropertiesD3DOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUAdapterPropertiesD3D(convertAdapterPropertiesD3D(env, obj));
}

const WGPUAdapterPropertiesD3D* convertAdapterPropertiesD3DArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUAdapterPropertiesD3D* entries = new WGPUAdapterPropertiesD3D[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertAdapterPropertiesD3D(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUAdapterPropertiesVk convertAdapterPropertiesVk(JNIEnv *env, jobject obj) {
    jclass adapterPropertiesVkClass = env->FindClass("android/dawn/AdapterPropertiesVk");

    WGPUAdapterPropertiesVk converted = {
        .chain = {.sType = WGPUSType_AdapterPropertiesVk},
        .driverVersion = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(adapterPropertiesVkClass, "getDriverVersion", "()I")))
    };

    return converted;
}

const WGPUAdapterPropertiesVk* convertAdapterPropertiesVkOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUAdapterPropertiesVk(convertAdapterPropertiesVk(env, obj));
}

const WGPUAdapterPropertiesVk* convertAdapterPropertiesVkArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUAdapterPropertiesVk* entries = new WGPUAdapterPropertiesVk[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertAdapterPropertiesVk(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBindGroupEntry convertBindGroupEntry(JNIEnv *env, jobject obj) {
    jclass bindGroupEntryClass = env->FindClass("android/dawn/BindGroupEntry");
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");
    jclass samplerClass = env->FindClass("android/dawn/Sampler");
    jmethodID samplerGetHandle = env->GetMethodID(samplerClass, "getHandle", "()J");
    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    jmethodID textureViewGetHandle = env->GetMethodID(textureViewClass, "getHandle", "()J");
    jobject buffer = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(bindGroupEntryClass, "getBuffer",
                                  "()Landroid/dawn/Buffer;")));
    WGPUBuffer nativeBuffer = buffer ?
        reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, bufferGetHandle)) : nullptr;
    jobject sampler = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(bindGroupEntryClass, "getSampler",
                                  "()Landroid/dawn/Sampler;")));
    WGPUSampler nativeSampler = sampler ?
        reinterpret_cast<WGPUSampler>(env->CallLongMethod(sampler, samplerGetHandle)) : nullptr;
    jobject textureView = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(bindGroupEntryClass, "getTextureView",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativeTextureView = textureView ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(textureView, textureViewGetHandle)) : nullptr;

    WGPUBindGroupEntry converted = {
        .binding = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(bindGroupEntryClass, "getBinding", "()I"))),
        .buffer = nativeBuffer,
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(bindGroupEntryClass, "getOffset", "()J"))),
        .size = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(bindGroupEntryClass, "getSize", "()J"))),
        .sampler = nativeSampler,
        .textureView = nativeTextureView
    };

    jclass ExternalTextureBindingEntryClass = env->FindClass("android/dawn/ExternalTextureBindingEntry");
    if (env->IsInstanceOf(obj, ExternalTextureBindingEntryClass)) {
         converted.nextInChain = &(new WGPUExternalTextureBindingEntry(convertExternalTextureBindingEntry(env, obj)))->chain;
    }
    return converted;
}

const WGPUBindGroupEntry* convertBindGroupEntryOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBindGroupEntry(convertBindGroupEntry(env, obj));
}

const WGPUBindGroupEntry* convertBindGroupEntryArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBindGroupEntry* entries = new WGPUBindGroupEntry[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBindGroupEntry(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBlendComponent convertBlendComponent(JNIEnv *env, jobject obj) {
    jclass blendComponentClass = env->FindClass("android/dawn/BlendComponent");

    WGPUBlendComponent converted = {
        .operation = static_cast<WGPUBlendOperation>(env->CallIntMethod(obj, env->GetMethodID(blendComponentClass, "getOperation", "()I"))),
        .srcFactor = static_cast<WGPUBlendFactor>(env->CallIntMethod(obj, env->GetMethodID(blendComponentClass, "getSrcFactor", "()I"))),
        .dstFactor = static_cast<WGPUBlendFactor>(env->CallIntMethod(obj, env->GetMethodID(blendComponentClass, "getDstFactor", "()I")))
    };

    return converted;
}

const WGPUBlendComponent* convertBlendComponentOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBlendComponent(convertBlendComponent(env, obj));
}

const WGPUBlendComponent* convertBlendComponentArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBlendComponent* entries = new WGPUBlendComponent[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBlendComponent(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBufferBindingLayout convertBufferBindingLayout(JNIEnv *env, jobject obj) {
    jclass bufferBindingLayoutClass = env->FindClass("android/dawn/BufferBindingLayout");

    WGPUBufferBindingLayout converted = {
        .type = static_cast<WGPUBufferBindingType>(env->CallIntMethod(obj, env->GetMethodID(bufferBindingLayoutClass, "getType", "()I"))),
        .hasDynamicOffset = env->CallBooleanMethod(obj, env->GetMethodID(bufferBindingLayoutClass, "getHasDynamicOffset", "()Z")),
        .minBindingSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(bufferBindingLayoutClass, "getMinBindingSize", "()J")))
    };

    return converted;
}

const WGPUBufferBindingLayout* convertBufferBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBufferBindingLayout(convertBufferBindingLayout(env, obj));
}

const WGPUBufferBindingLayout* convertBufferBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBufferBindingLayout* entries = new WGPUBufferBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBufferBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBufferDescriptor convertBufferDescriptor(JNIEnv *env, jobject obj) {
    jclass bufferDescriptorClass = env->FindClass("android/dawn/BufferDescriptor");

    WGPUBufferDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(bufferDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .usage = static_cast<WGPUBufferUsage>(env->CallIntMethod(obj, env->GetMethodID(bufferDescriptorClass, "getUsage", "()I"))),
        .size = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(bufferDescriptorClass, "getSize", "()J"))),
        .mappedAtCreation = env->CallBooleanMethod(obj, env->GetMethodID(bufferDescriptorClass, "getMappedAtCreation", "()Z"))
    };

    jclass BufferHostMappedPointerClass = env->FindClass("android/dawn/BufferHostMappedPointer");
    if (env->IsInstanceOf(obj, BufferHostMappedPointerClass)) {
         converted.nextInChain = &(new WGPUBufferHostMappedPointer(convertBufferHostMappedPointer(env, obj)))->chain;
    }
    jclass DawnBufferDescriptorErrorInfoFromWireClientClass = env->FindClass("android/dawn/DawnBufferDescriptorErrorInfoFromWireClient");
    if (env->IsInstanceOf(obj, DawnBufferDescriptorErrorInfoFromWireClientClass)) {
         converted.nextInChain = &(new WGPUDawnBufferDescriptorErrorInfoFromWireClient(convertDawnBufferDescriptorErrorInfoFromWireClient(env, obj)))->chain;
    }
    return converted;
}

const WGPUBufferDescriptor* convertBufferDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBufferDescriptor(convertBufferDescriptor(env, obj));
}

const WGPUBufferDescriptor* convertBufferDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBufferDescriptor* entries = new WGPUBufferDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBufferDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBufferHostMappedPointer convertBufferHostMappedPointer(JNIEnv *env, jobject obj) {
    jclass bufferHostMappedPointerClass = env->FindClass("android/dawn/BufferHostMappedPointer");

    WGPUBufferHostMappedPointer converted = {
        .chain = {.sType = WGPUSType_BufferHostMappedPointer},
        .pointer = nullptr /* unknown. annotation: value category: native name: void * */,
        .disposeCallback = nullptr /* unknown. annotation: value category: function pointer name: callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUBufferHostMappedPointer* convertBufferHostMappedPointerOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBufferHostMappedPointer(convertBufferHostMappedPointer(env, obj));
}

const WGPUBufferHostMappedPointer* convertBufferHostMappedPointerArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBufferHostMappedPointer* entries = new WGPUBufferHostMappedPointer[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBufferHostMappedPointer(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBufferMapCallbackInfo convertBufferMapCallbackInfo(JNIEnv *env, jobject obj) {
    jclass bufferMapCallbackInfoClass = env->FindClass("android/dawn/BufferMapCallbackInfo");

    WGPUBufferMapCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(bufferMapCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: buffer map callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUBufferMapCallbackInfo* convertBufferMapCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBufferMapCallbackInfo(convertBufferMapCallbackInfo(env, obj));
}

const WGPUBufferMapCallbackInfo* convertBufferMapCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBufferMapCallbackInfo* entries = new WGPUBufferMapCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBufferMapCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUColor convertColor(JNIEnv *env, jobject obj) {
    jclass colorClass = env->FindClass("android/dawn/Color");

    WGPUColor converted = {
        .r = env->CallDoubleMethod(obj, env->GetMethodID(colorClass, "getR", "()D")),
        .g = env->CallDoubleMethod(obj, env->GetMethodID(colorClass, "getG", "()D")),
        .b = env->CallDoubleMethod(obj, env->GetMethodID(colorClass, "getB", "()D")),
        .a = env->CallDoubleMethod(obj, env->GetMethodID(colorClass, "getA", "()D"))
    };

    return converted;
}

const WGPUColor* convertColorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUColor(convertColor(env, obj));
}

const WGPUColor* convertColorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUColor* entries = new WGPUColor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertColor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCommandBufferDescriptor convertCommandBufferDescriptor(JNIEnv *env, jobject obj) {
    jclass commandBufferDescriptorClass = env->FindClass("android/dawn/CommandBufferDescriptor");

    WGPUCommandBufferDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(commandBufferDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPUCommandBufferDescriptor* convertCommandBufferDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCommandBufferDescriptor(convertCommandBufferDescriptor(env, obj));
}

const WGPUCommandBufferDescriptor* convertCommandBufferDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCommandBufferDescriptor* entries = new WGPUCommandBufferDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCommandBufferDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCommandEncoderDescriptor convertCommandEncoderDescriptor(JNIEnv *env, jobject obj) {
    jclass commandEncoderDescriptorClass = env->FindClass("android/dawn/CommandEncoderDescriptor");

    WGPUCommandEncoderDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(commandEncoderDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    jclass DawnEncoderInternalUsageDescriptorClass = env->FindClass("android/dawn/DawnEncoderInternalUsageDescriptor");
    if (env->IsInstanceOf(obj, DawnEncoderInternalUsageDescriptorClass)) {
         converted.nextInChain = &(new WGPUDawnEncoderInternalUsageDescriptor(convertDawnEncoderInternalUsageDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUCommandEncoderDescriptor* convertCommandEncoderDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCommandEncoderDescriptor(convertCommandEncoderDescriptor(env, obj));
}

const WGPUCommandEncoderDescriptor* convertCommandEncoderDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCommandEncoderDescriptor* entries = new WGPUCommandEncoderDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCommandEncoderDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCompilationInfoCallbackInfo convertCompilationInfoCallbackInfo(JNIEnv *env, jobject obj) {
    jclass compilationInfoCallbackInfoClass = env->FindClass("android/dawn/CompilationInfoCallbackInfo");

    WGPUCompilationInfoCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(compilationInfoCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: compilation info callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUCompilationInfoCallbackInfo* convertCompilationInfoCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCompilationInfoCallbackInfo(convertCompilationInfoCallbackInfo(env, obj));
}

const WGPUCompilationInfoCallbackInfo* convertCompilationInfoCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCompilationInfoCallbackInfo* entries = new WGPUCompilationInfoCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCompilationInfoCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCompilationMessage convertCompilationMessage(JNIEnv *env, jobject obj) {
    jclass compilationMessageClass = env->FindClass("android/dawn/CompilationMessage");

    WGPUCompilationMessage converted = {
        .message = getString(env, env->CallObjectMethod(obj, env->GetMethodID(compilationMessageClass, "getMessage", "()Ljava/lang/String;"))),
        .type = static_cast<WGPUCompilationMessageType>(env->CallIntMethod(obj, env->GetMethodID(compilationMessageClass, "getType", "()I"))),
        .lineNum = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getLineNum", "()J"))),
        .linePos = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getLinePos", "()J"))),
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getOffset", "()J"))),
        .length = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getLength", "()J"))),
        .utf16LinePos = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getUtf16LinePos", "()J"))),
        .utf16Offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getUtf16Offset", "()J"))),
        .utf16Length = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(compilationMessageClass, "getUtf16Length", "()J")))
    };

    return converted;
}

const WGPUCompilationMessage* convertCompilationMessageOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCompilationMessage(convertCompilationMessage(env, obj));
}

const WGPUCompilationMessage* convertCompilationMessageArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCompilationMessage* entries = new WGPUCompilationMessage[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCompilationMessage(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUComputePassTimestampWrites convertComputePassTimestampWrites(JNIEnv *env, jobject obj) {
    jclass computePassTimestampWritesClass = env->FindClass("android/dawn/ComputePassTimestampWrites");
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");
    jobject querySet = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(computePassTimestampWritesClass, "getQuerySet",
                                  "()Landroid/dawn/QuerySet;")));
    WGPUQuerySet nativeQuerySet = querySet ?
        reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, querySetGetHandle)) : nullptr;

    WGPUComputePassTimestampWrites converted = {
        .querySet = nativeQuerySet,
        .beginningOfPassWriteIndex = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(computePassTimestampWritesClass, "getBeginningOfPassWriteIndex", "()I"))),
        .endOfPassWriteIndex = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(computePassTimestampWritesClass, "getEndOfPassWriteIndex", "()I")))
    };

    return converted;
}

const WGPUComputePassTimestampWrites* convertComputePassTimestampWritesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUComputePassTimestampWrites(convertComputePassTimestampWrites(env, obj));
}

const WGPUComputePassTimestampWrites* convertComputePassTimestampWritesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUComputePassTimestampWrites* entries = new WGPUComputePassTimestampWrites[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertComputePassTimestampWrites(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUConstantEntry convertConstantEntry(JNIEnv *env, jobject obj) {
    jclass constantEntryClass = env->FindClass("android/dawn/ConstantEntry");

    WGPUConstantEntry converted = {
        .key = getString(env, env->CallObjectMethod(obj, env->GetMethodID(constantEntryClass, "getKey", "()Ljava/lang/String;"))),
        .value = env->CallDoubleMethod(obj, env->GetMethodID(constantEntryClass, "getValue", "()D"))
    };

    return converted;
}

const WGPUConstantEntry* convertConstantEntryOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUConstantEntry(convertConstantEntry(env, obj));
}

const WGPUConstantEntry* convertConstantEntryArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUConstantEntry* entries = new WGPUConstantEntry[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertConstantEntry(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCopyTextureForBrowserOptions convertCopyTextureForBrowserOptions(JNIEnv *env, jobject obj) {
    jclass copyTextureForBrowserOptionsClass = env->FindClass("android/dawn/CopyTextureForBrowserOptions");
    jobject nativeSrcTransferFunctionParameters = env->CallObjectMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getSrcTransferFunctionParameters", "()[F"));
    jobject nativeConversionMatrix = env->CallObjectMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getConversionMatrix", "()[F"));
    jobject nativeDstTransferFunctionParameters = env->CallObjectMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getDstTransferFunctionParameters", "()[F"));

    WGPUCopyTextureForBrowserOptions converted = {
        .flipY = env->CallBooleanMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getFlipY", "()Z")),
        .needsColorSpaceConversion = env->CallBooleanMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getNeedsColorSpaceConversion", "()Z")),
        .srcAlphaMode = static_cast<WGPUAlphaMode>(env->CallIntMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getSrcAlphaMode", "()I"))),
        .srcTransferFunctionParameters = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeSrcTransferFunctionParameters), 0),
        .conversionMatrix = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeConversionMatrix), 0),
        .dstTransferFunctionParameters = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeDstTransferFunctionParameters), 0),
        .dstAlphaMode = static_cast<WGPUAlphaMode>(env->CallIntMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getDstAlphaMode", "()I"))),
        .internalUsage = env->CallBooleanMethod(obj, env->GetMethodID(copyTextureForBrowserOptionsClass, "getInternalUsage", "()Z"))
    };

    return converted;
}

const WGPUCopyTextureForBrowserOptions* convertCopyTextureForBrowserOptionsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCopyTextureForBrowserOptions(convertCopyTextureForBrowserOptions(env, obj));
}

const WGPUCopyTextureForBrowserOptions* convertCopyTextureForBrowserOptionsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCopyTextureForBrowserOptions* entries = new WGPUCopyTextureForBrowserOptions[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCopyTextureForBrowserOptions(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCreateComputePipelineAsyncCallbackInfo convertCreateComputePipelineAsyncCallbackInfo(JNIEnv *env, jobject obj) {
    jclass createComputePipelineAsyncCallbackInfoClass = env->FindClass("android/dawn/CreateComputePipelineAsyncCallbackInfo");

    WGPUCreateComputePipelineAsyncCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(createComputePipelineAsyncCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: create compute pipeline async callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUCreateComputePipelineAsyncCallbackInfo* convertCreateComputePipelineAsyncCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCreateComputePipelineAsyncCallbackInfo(convertCreateComputePipelineAsyncCallbackInfo(env, obj));
}

const WGPUCreateComputePipelineAsyncCallbackInfo* convertCreateComputePipelineAsyncCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCreateComputePipelineAsyncCallbackInfo* entries = new WGPUCreateComputePipelineAsyncCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCreateComputePipelineAsyncCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCreateRenderPipelineAsyncCallbackInfo convertCreateRenderPipelineAsyncCallbackInfo(JNIEnv *env, jobject obj) {
    jclass createRenderPipelineAsyncCallbackInfoClass = env->FindClass("android/dawn/CreateRenderPipelineAsyncCallbackInfo");

    WGPUCreateRenderPipelineAsyncCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(createRenderPipelineAsyncCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: create render pipeline async callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUCreateRenderPipelineAsyncCallbackInfo* convertCreateRenderPipelineAsyncCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCreateRenderPipelineAsyncCallbackInfo(convertCreateRenderPipelineAsyncCallbackInfo(env, obj));
}

const WGPUCreateRenderPipelineAsyncCallbackInfo* convertCreateRenderPipelineAsyncCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCreateRenderPipelineAsyncCallbackInfo* entries = new WGPUCreateRenderPipelineAsyncCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCreateRenderPipelineAsyncCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnAdapterPropertiesPowerPreference convertDawnAdapterPropertiesPowerPreference(JNIEnv *env, jobject obj) {
    jclass dawnAdapterPropertiesPowerPreferenceClass = env->FindClass("android/dawn/DawnAdapterPropertiesPowerPreference");

    WGPUDawnAdapterPropertiesPowerPreference converted = {
        .chain = {.sType = WGPUSType_DawnAdapterPropertiesPowerPreference},
        .powerPreference = static_cast<WGPUPowerPreference>(env->CallIntMethod(obj, env->GetMethodID(dawnAdapterPropertiesPowerPreferenceClass, "getPowerPreference", "()I")))
    };

    return converted;
}

const WGPUDawnAdapterPropertiesPowerPreference* convertDawnAdapterPropertiesPowerPreferenceOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnAdapterPropertiesPowerPreference(convertDawnAdapterPropertiesPowerPreference(env, obj));
}

const WGPUDawnAdapterPropertiesPowerPreference* convertDawnAdapterPropertiesPowerPreferenceArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnAdapterPropertiesPowerPreference* entries = new WGPUDawnAdapterPropertiesPowerPreference[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnAdapterPropertiesPowerPreference(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnBufferDescriptorErrorInfoFromWireClient convertDawnBufferDescriptorErrorInfoFromWireClient(JNIEnv *env, jobject obj) {
    jclass dawnBufferDescriptorErrorInfoFromWireClientClass = env->FindClass("android/dawn/DawnBufferDescriptorErrorInfoFromWireClient");

    WGPUDawnBufferDescriptorErrorInfoFromWireClient converted = {
        .chain = {.sType = WGPUSType_DawnBufferDescriptorErrorInfoFromWireClient},
        .outOfMemory = env->CallBooleanMethod(obj, env->GetMethodID(dawnBufferDescriptorErrorInfoFromWireClientClass, "getOutOfMemory", "()Z"))
    };

    return converted;
}

const WGPUDawnBufferDescriptorErrorInfoFromWireClient* convertDawnBufferDescriptorErrorInfoFromWireClientOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnBufferDescriptorErrorInfoFromWireClient(convertDawnBufferDescriptorErrorInfoFromWireClient(env, obj));
}

const WGPUDawnBufferDescriptorErrorInfoFromWireClient* convertDawnBufferDescriptorErrorInfoFromWireClientArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnBufferDescriptorErrorInfoFromWireClient* entries = new WGPUDawnBufferDescriptorErrorInfoFromWireClient[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnBufferDescriptorErrorInfoFromWireClient(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnCacheDeviceDescriptor convertDawnCacheDeviceDescriptor(JNIEnv *env, jobject obj) {
    jclass dawnCacheDeviceDescriptorClass = env->FindClass("android/dawn/DawnCacheDeviceDescriptor");

    WGPUDawnCacheDeviceDescriptor converted = {
        .chain = {.sType = WGPUSType_DawnCacheDeviceDescriptor},
        .isolationKey = getString(env, env->CallObjectMethod(obj, env->GetMethodID(dawnCacheDeviceDescriptorClass, "getIsolationKey", "()Ljava/lang/String;"))),
        .loadDataFunction = nullptr /* unknown. annotation: value category: function pointer name: dawn load cache data function */,
        .storeDataFunction = nullptr /* unknown. annotation: value category: function pointer name: dawn store cache data function */,
        .functionUserdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUDawnCacheDeviceDescriptor* convertDawnCacheDeviceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnCacheDeviceDescriptor(convertDawnCacheDeviceDescriptor(env, obj));
}

const WGPUDawnCacheDeviceDescriptor* convertDawnCacheDeviceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnCacheDeviceDescriptor* entries = new WGPUDawnCacheDeviceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnCacheDeviceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnComputePipelineFullSubgroups convertDawnComputePipelineFullSubgroups(JNIEnv *env, jobject obj) {
    jclass dawnComputePipelineFullSubgroupsClass = env->FindClass("android/dawn/DawnComputePipelineFullSubgroups");

    WGPUDawnComputePipelineFullSubgroups converted = {
        .chain = {.sType = WGPUSType_DawnComputePipelineFullSubgroups},
        .requiresFullSubgroups = env->CallBooleanMethod(obj, env->GetMethodID(dawnComputePipelineFullSubgroupsClass, "getRequiresFullSubgroups", "()Z"))
    };

    return converted;
}

const WGPUDawnComputePipelineFullSubgroups* convertDawnComputePipelineFullSubgroupsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnComputePipelineFullSubgroups(convertDawnComputePipelineFullSubgroups(env, obj));
}

const WGPUDawnComputePipelineFullSubgroups* convertDawnComputePipelineFullSubgroupsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnComputePipelineFullSubgroups* entries = new WGPUDawnComputePipelineFullSubgroups[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnComputePipelineFullSubgroups(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnEncoderInternalUsageDescriptor convertDawnEncoderInternalUsageDescriptor(JNIEnv *env, jobject obj) {
    jclass dawnEncoderInternalUsageDescriptorClass = env->FindClass("android/dawn/DawnEncoderInternalUsageDescriptor");

    WGPUDawnEncoderInternalUsageDescriptor converted = {
        .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
        .useInternalUsages = env->CallBooleanMethod(obj, env->GetMethodID(dawnEncoderInternalUsageDescriptorClass, "getUseInternalUsages", "()Z"))
    };

    return converted;
}

const WGPUDawnEncoderInternalUsageDescriptor* convertDawnEncoderInternalUsageDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnEncoderInternalUsageDescriptor(convertDawnEncoderInternalUsageDescriptor(env, obj));
}

const WGPUDawnEncoderInternalUsageDescriptor* convertDawnEncoderInternalUsageDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnEncoderInternalUsageDescriptor* entries = new WGPUDawnEncoderInternalUsageDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnEncoderInternalUsageDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnExperimentalSubgroupLimits convertDawnExperimentalSubgroupLimits(JNIEnv *env, jobject obj) {
    jclass dawnExperimentalSubgroupLimitsClass = env->FindClass("android/dawn/DawnExperimentalSubgroupLimits");

    WGPUDawnExperimentalSubgroupLimits converted = {
        .chain = {.sType = WGPUSType_DawnExperimentalSubgroupLimits},
        .minSubgroupSize = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(dawnExperimentalSubgroupLimitsClass, "getMinSubgroupSize", "()I"))),
        .maxSubgroupSize = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(dawnExperimentalSubgroupLimitsClass, "getMaxSubgroupSize", "()I")))
    };

    return converted;
}

const WGPUDawnExperimentalSubgroupLimits* convertDawnExperimentalSubgroupLimitsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnExperimentalSubgroupLimits(convertDawnExperimentalSubgroupLimits(env, obj));
}

const WGPUDawnExperimentalSubgroupLimits* convertDawnExperimentalSubgroupLimitsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnExperimentalSubgroupLimits* entries = new WGPUDawnExperimentalSubgroupLimits[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnExperimentalSubgroupLimits(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnMultisampleStateRenderToSingleSampled convertDawnMultisampleStateRenderToSingleSampled(JNIEnv *env, jobject obj) {
    jclass dawnMultisampleStateRenderToSingleSampledClass = env->FindClass("android/dawn/DawnMultisampleStateRenderToSingleSampled");

    WGPUDawnMultisampleStateRenderToSingleSampled converted = {
        .chain = {.sType = WGPUSType_DawnMultisampleStateRenderToSingleSampled},
        .enabled = env->CallBooleanMethod(obj, env->GetMethodID(dawnMultisampleStateRenderToSingleSampledClass, "getEnabled", "()Z"))
    };

    return converted;
}

const WGPUDawnMultisampleStateRenderToSingleSampled* convertDawnMultisampleStateRenderToSingleSampledOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnMultisampleStateRenderToSingleSampled(convertDawnMultisampleStateRenderToSingleSampled(env, obj));
}

const WGPUDawnMultisampleStateRenderToSingleSampled* convertDawnMultisampleStateRenderToSingleSampledArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnMultisampleStateRenderToSingleSampled* entries = new WGPUDawnMultisampleStateRenderToSingleSampled[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnMultisampleStateRenderToSingleSampled(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnRenderPassColorAttachmentRenderToSingleSampled convertDawnRenderPassColorAttachmentRenderToSingleSampled(JNIEnv *env, jobject obj) {
    jclass dawnRenderPassColorAttachmentRenderToSingleSampledClass = env->FindClass("android/dawn/DawnRenderPassColorAttachmentRenderToSingleSampled");

    WGPUDawnRenderPassColorAttachmentRenderToSingleSampled converted = {
        .chain = {.sType = WGPUSType_DawnRenderPassColorAttachmentRenderToSingleSampled},
        .implicitSampleCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(dawnRenderPassColorAttachmentRenderToSingleSampledClass, "getImplicitSampleCount", "()I")))
    };

    return converted;
}

const WGPUDawnRenderPassColorAttachmentRenderToSingleSampled* convertDawnRenderPassColorAttachmentRenderToSingleSampledOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnRenderPassColorAttachmentRenderToSingleSampled(convertDawnRenderPassColorAttachmentRenderToSingleSampled(env, obj));
}

const WGPUDawnRenderPassColorAttachmentRenderToSingleSampled* convertDawnRenderPassColorAttachmentRenderToSingleSampledArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnRenderPassColorAttachmentRenderToSingleSampled* entries = new WGPUDawnRenderPassColorAttachmentRenderToSingleSampled[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnRenderPassColorAttachmentRenderToSingleSampled(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnShaderModuleSPIRVOptionsDescriptor convertDawnShaderModuleSPIRVOptionsDescriptor(JNIEnv *env, jobject obj) {
    jclass dawnShaderModuleSPIRVOptionsDescriptorClass = env->FindClass("android/dawn/DawnShaderModuleSPIRVOptionsDescriptor");

    WGPUDawnShaderModuleSPIRVOptionsDescriptor converted = {
        .chain = {.sType = WGPUSType_DawnShaderModuleSPIRVOptionsDescriptor},
        .allowNonUniformDerivatives = env->CallBooleanMethod(obj, env->GetMethodID(dawnShaderModuleSPIRVOptionsDescriptorClass, "getAllowNonUniformDerivatives", "()Z"))
    };

    return converted;
}

const WGPUDawnShaderModuleSPIRVOptionsDescriptor* convertDawnShaderModuleSPIRVOptionsDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnShaderModuleSPIRVOptionsDescriptor(convertDawnShaderModuleSPIRVOptionsDescriptor(env, obj));
}

const WGPUDawnShaderModuleSPIRVOptionsDescriptor* convertDawnShaderModuleSPIRVOptionsDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnShaderModuleSPIRVOptionsDescriptor* entries = new WGPUDawnShaderModuleSPIRVOptionsDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnShaderModuleSPIRVOptionsDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnTextureInternalUsageDescriptor convertDawnTextureInternalUsageDescriptor(JNIEnv *env, jobject obj) {
    jclass dawnTextureInternalUsageDescriptorClass = env->FindClass("android/dawn/DawnTextureInternalUsageDescriptor");

    WGPUDawnTextureInternalUsageDescriptor converted = {
        .chain = {.sType = WGPUSType_DawnTextureInternalUsageDescriptor},
        .internalUsage = static_cast<WGPUTextureUsage>(env->CallIntMethod(obj, env->GetMethodID(dawnTextureInternalUsageDescriptorClass, "getInternalUsage", "()I")))
    };

    return converted;
}

const WGPUDawnTextureInternalUsageDescriptor* convertDawnTextureInternalUsageDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnTextureInternalUsageDescriptor(convertDawnTextureInternalUsageDescriptor(env, obj));
}

const WGPUDawnTextureInternalUsageDescriptor* convertDawnTextureInternalUsageDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnTextureInternalUsageDescriptor* entries = new WGPUDawnTextureInternalUsageDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnTextureInternalUsageDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDawnWireWGSLControl convertDawnWireWGSLControl(JNIEnv *env, jobject obj) {
    jclass dawnWireWGSLControlClass = env->FindClass("android/dawn/DawnWireWGSLControl");

    WGPUDawnWireWGSLControl converted = {
        .chain = {.sType = WGPUSType_DawnWireWGSLControl},
        .enableExperimental = env->CallBooleanMethod(obj, env->GetMethodID(dawnWireWGSLControlClass, "getEnableExperimental", "()Z")),
        .enableUnsafe = env->CallBooleanMethod(obj, env->GetMethodID(dawnWireWGSLControlClass, "getEnableUnsafe", "()Z")),
        .enableTesting = env->CallBooleanMethod(obj, env->GetMethodID(dawnWireWGSLControlClass, "getEnableTesting", "()Z"))
    };

    return converted;
}

const WGPUDawnWireWGSLControl* convertDawnWireWGSLControlOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDawnWireWGSLControl(convertDawnWireWGSLControl(env, obj));
}

const WGPUDawnWireWGSLControl* convertDawnWireWGSLControlArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDawnWireWGSLControl* entries = new WGPUDawnWireWGSLControl[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDawnWireWGSLControl(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDepthStencilStateDepthWriteDefinedDawn convertDepthStencilStateDepthWriteDefinedDawn(JNIEnv *env, jobject obj) {
    jclass depthStencilStateDepthWriteDefinedDawnClass = env->FindClass("android/dawn/DepthStencilStateDepthWriteDefinedDawn");

    WGPUDepthStencilStateDepthWriteDefinedDawn converted = {
        .chain = {.sType = WGPUSType_DepthStencilStateDepthWriteDefinedDawn},
        .depthWriteDefined = env->CallBooleanMethod(obj, env->GetMethodID(depthStencilStateDepthWriteDefinedDawnClass, "getDepthWriteDefined", "()Z"))
    };

    return converted;
}

const WGPUDepthStencilStateDepthWriteDefinedDawn* convertDepthStencilStateDepthWriteDefinedDawnOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDepthStencilStateDepthWriteDefinedDawn(convertDepthStencilStateDepthWriteDefinedDawn(env, obj));
}

const WGPUDepthStencilStateDepthWriteDefinedDawn* convertDepthStencilStateDepthWriteDefinedDawnArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDepthStencilStateDepthWriteDefinedDawn* entries = new WGPUDepthStencilStateDepthWriteDefinedDawn[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDepthStencilStateDepthWriteDefinedDawn(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDrmFormatProperties convertDrmFormatProperties(JNIEnv *env, jobject obj) {
    jclass drmFormatPropertiesClass = env->FindClass("android/dawn/DrmFormatProperties");

    WGPUDrmFormatProperties converted = {
        .modifier = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(drmFormatPropertiesClass, "getModifier", "()J"))),
        .modifierPlaneCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(drmFormatPropertiesClass, "getModifierPlaneCount", "()I")))
    };

    return converted;
}

const WGPUDrmFormatProperties* convertDrmFormatPropertiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDrmFormatProperties(convertDrmFormatProperties(env, obj));
}

const WGPUDrmFormatProperties* convertDrmFormatPropertiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDrmFormatProperties* entries = new WGPUDrmFormatProperties[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDrmFormatProperties(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUExtent2D convertExtent2D(JNIEnv *env, jobject obj) {
    jclass extent2DClass = env->FindClass("android/dawn/Extent2D");

    WGPUExtent2D converted = {
        .width = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(extent2DClass, "getWidth", "()I"))),
        .height = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(extent2DClass, "getHeight", "()I")))
    };

    return converted;
}

const WGPUExtent2D* convertExtent2DOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUExtent2D(convertExtent2D(env, obj));
}

const WGPUExtent2D* convertExtent2DArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUExtent2D* entries = new WGPUExtent2D[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertExtent2D(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUExtent3D convertExtent3D(JNIEnv *env, jobject obj) {
    jclass extent3DClass = env->FindClass("android/dawn/Extent3D");

    WGPUExtent3D converted = {
        .width = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(extent3DClass, "getWidth", "()I"))),
        .height = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(extent3DClass, "getHeight", "()I"))),
        .depthOrArrayLayers = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(extent3DClass, "getDepthOrArrayLayers", "()I")))
    };

    return converted;
}

const WGPUExtent3D* convertExtent3DOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUExtent3D(convertExtent3D(env, obj));
}

const WGPUExtent3D* convertExtent3DArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUExtent3D* entries = new WGPUExtent3D[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertExtent3D(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUExternalTextureBindingEntry convertExternalTextureBindingEntry(JNIEnv *env, jobject obj) {
    jclass externalTextureBindingEntryClass = env->FindClass("android/dawn/ExternalTextureBindingEntry");
    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    jmethodID externalTextureGetHandle = env->GetMethodID(externalTextureClass, "getHandle", "()J");
    jobject externalTexture = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(externalTextureBindingEntryClass, "getExternalTexture",
                                  "()Landroid/dawn/ExternalTexture;")));
    WGPUExternalTexture nativeExternalTexture = externalTexture ?
        reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(externalTexture, externalTextureGetHandle)) : nullptr;

    WGPUExternalTextureBindingEntry converted = {
        .chain = {.sType = WGPUSType_ExternalTextureBindingEntry},
        .externalTexture = nativeExternalTexture
    };

    return converted;
}

const WGPUExternalTextureBindingEntry* convertExternalTextureBindingEntryOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUExternalTextureBindingEntry(convertExternalTextureBindingEntry(env, obj));
}

const WGPUExternalTextureBindingEntry* convertExternalTextureBindingEntryArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUExternalTextureBindingEntry* entries = new WGPUExternalTextureBindingEntry[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertExternalTextureBindingEntry(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUExternalTextureBindingLayout convertExternalTextureBindingLayout(JNIEnv *env, jobject obj) {
    jclass externalTextureBindingLayoutClass = env->FindClass("android/dawn/ExternalTextureBindingLayout");

    WGPUExternalTextureBindingLayout converted = {
        .chain = {.sType = WGPUSType_ExternalTextureBindingLayout},
    };

    return converted;
}

const WGPUExternalTextureBindingLayout* convertExternalTextureBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUExternalTextureBindingLayout(convertExternalTextureBindingLayout(env, obj));
}

const WGPUExternalTextureBindingLayout* convertExternalTextureBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUExternalTextureBindingLayout* entries = new WGPUExternalTextureBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertExternalTextureBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUFormatCapabilities convertFormatCapabilities(JNIEnv *env, jobject obj) {
    jclass formatCapabilitiesClass = env->FindClass("android/dawn/FormatCapabilities");

    WGPUFormatCapabilities converted = {
    };

    jclass DrmFormatCapabilitiesClass = env->FindClass("android/dawn/DrmFormatCapabilities");
    if (env->IsInstanceOf(obj, DrmFormatCapabilitiesClass)) {
         converted.nextInChain = &(new WGPUDrmFormatCapabilities(convertDrmFormatCapabilities(env, obj)))->chain;
    }
    return converted;
}

const WGPUFormatCapabilities* convertFormatCapabilitiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUFormatCapabilities(convertFormatCapabilities(env, obj));
}

const WGPUFormatCapabilities* convertFormatCapabilitiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUFormatCapabilities* entries = new WGPUFormatCapabilities[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertFormatCapabilities(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUFuture convertFuture(JNIEnv *env, jobject obj) {
    jclass futureClass = env->FindClass("android/dawn/Future");

    WGPUFuture converted = {
        .id = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(futureClass, "getId", "()J")))
    };

    return converted;
}

const WGPUFuture* convertFutureOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUFuture(convertFuture(env, obj));
}

const WGPUFuture* convertFutureArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUFuture* entries = new WGPUFuture[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertFuture(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUInstanceFeatures convertInstanceFeatures(JNIEnv *env, jobject obj) {
    jclass instanceFeaturesClass = env->FindClass("android/dawn/InstanceFeatures");

    WGPUInstanceFeatures converted = {
        .timedWaitAnyEnable = env->CallBooleanMethod(obj, env->GetMethodID(instanceFeaturesClass, "getTimedWaitAnyEnable", "()Z")),
        .timedWaitAnyMaxCount = static_cast<size_t>(env->CallLongMethod(obj, env->GetMethodID(instanceFeaturesClass, "getTimedWaitAnyMaxCount", "()J")))
    };

    return converted;
}

const WGPUInstanceFeatures* convertInstanceFeaturesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUInstanceFeatures(convertInstanceFeatures(env, obj));
}

const WGPUInstanceFeatures* convertInstanceFeaturesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUInstanceFeatures* entries = new WGPUInstanceFeatures[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertInstanceFeatures(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPULimits convertLimits(JNIEnv *env, jobject obj) {
    jclass limitsClass = env->FindClass("android/dawn/Limits");

    WGPULimits converted = {
        .maxTextureDimension1D = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxTextureDimension1D", "()I"))),
        .maxTextureDimension2D = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxTextureDimension2D", "()I"))),
        .maxTextureDimension3D = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxTextureDimension3D", "()I"))),
        .maxTextureArrayLayers = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxTextureArrayLayers", "()I"))),
        .maxBindGroups = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxBindGroups", "()I"))),
        .maxBindGroupsPlusVertexBuffers = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxBindGroupsPlusVertexBuffers", "()I"))),
        .maxBindingsPerBindGroup = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxBindingsPerBindGroup", "()I"))),
        .maxDynamicUniformBuffersPerPipelineLayout = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxDynamicUniformBuffersPerPipelineLayout", "()I"))),
        .maxDynamicStorageBuffersPerPipelineLayout = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxDynamicStorageBuffersPerPipelineLayout", "()I"))),
        .maxSampledTexturesPerShaderStage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxSampledTexturesPerShaderStage", "()I"))),
        .maxSamplersPerShaderStage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxSamplersPerShaderStage", "()I"))),
        .maxStorageBuffersPerShaderStage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxStorageBuffersPerShaderStage", "()I"))),
        .maxStorageTexturesPerShaderStage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxStorageTexturesPerShaderStage", "()I"))),
        .maxUniformBuffersPerShaderStage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxUniformBuffersPerShaderStage", "()I"))),
        .maxUniformBufferBindingSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(limitsClass, "getMaxUniformBufferBindingSize", "()J"))),
        .maxStorageBufferBindingSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(limitsClass, "getMaxStorageBufferBindingSize", "()J"))),
        .minUniformBufferOffsetAlignment = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMinUniformBufferOffsetAlignment", "()I"))),
        .minStorageBufferOffsetAlignment = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMinStorageBufferOffsetAlignment", "()I"))),
        .maxVertexBuffers = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxVertexBuffers", "()I"))),
        .maxBufferSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(limitsClass, "getMaxBufferSize", "()J"))),
        .maxVertexAttributes = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxVertexAttributes", "()I"))),
        .maxVertexBufferArrayStride = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxVertexBufferArrayStride", "()I"))),
        .maxInterStageShaderComponents = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxInterStageShaderComponents", "()I"))),
        .maxInterStageShaderVariables = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxInterStageShaderVariables", "()I"))),
        .maxColorAttachments = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxColorAttachments", "()I"))),
        .maxColorAttachmentBytesPerSample = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxColorAttachmentBytesPerSample", "()I"))),
        .maxComputeWorkgroupStorageSize = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeWorkgroupStorageSize", "()I"))),
        .maxComputeInvocationsPerWorkgroup = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeInvocationsPerWorkgroup", "()I"))),
        .maxComputeWorkgroupSizeX = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeWorkgroupSizeX", "()I"))),
        .maxComputeWorkgroupSizeY = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeWorkgroupSizeY", "()I"))),
        .maxComputeWorkgroupSizeZ = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeWorkgroupSizeZ", "()I"))),
        .maxComputeWorkgroupsPerDimension = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(limitsClass, "getMaxComputeWorkgroupsPerDimension", "()I")))
    };

    return converted;
}

const WGPULimits* convertLimitsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPULimits(convertLimits(env, obj));
}

const WGPULimits* convertLimitsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPULimits* entries = new WGPULimits[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertLimits(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUMemoryHeapInfo convertMemoryHeapInfo(JNIEnv *env, jobject obj) {
    jclass memoryHeapInfoClass = env->FindClass("android/dawn/MemoryHeapInfo");

    WGPUMemoryHeapInfo converted = {
        .properties = static_cast<WGPUHeapProperty>(env->CallIntMethod(obj, env->GetMethodID(memoryHeapInfoClass, "getProperties", "()I"))),
        .size = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(memoryHeapInfoClass, "getSize", "()J")))
    };

    return converted;
}

const WGPUMemoryHeapInfo* convertMemoryHeapInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUMemoryHeapInfo(convertMemoryHeapInfo(env, obj));
}

const WGPUMemoryHeapInfo* convertMemoryHeapInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUMemoryHeapInfo* entries = new WGPUMemoryHeapInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertMemoryHeapInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUMultisampleState convertMultisampleState(JNIEnv *env, jobject obj) {
    jclass multisampleStateClass = env->FindClass("android/dawn/MultisampleState");

    WGPUMultisampleState converted = {
        .count = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(multisampleStateClass, "getCount", "()I"))),
        .mask = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(multisampleStateClass, "getMask", "()I"))),
        .alphaToCoverageEnabled = env->CallBooleanMethod(obj, env->GetMethodID(multisampleStateClass, "getAlphaToCoverageEnabled", "()Z"))
    };

    jclass DawnMultisampleStateRenderToSingleSampledClass = env->FindClass("android/dawn/DawnMultisampleStateRenderToSingleSampled");
    if (env->IsInstanceOf(obj, DawnMultisampleStateRenderToSingleSampledClass)) {
         converted.nextInChain = &(new WGPUDawnMultisampleStateRenderToSingleSampled(convertDawnMultisampleStateRenderToSingleSampled(env, obj)))->chain;
    }
    return converted;
}

const WGPUMultisampleState* convertMultisampleStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUMultisampleState(convertMultisampleState(env, obj));
}

const WGPUMultisampleState* convertMultisampleStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUMultisampleState* entries = new WGPUMultisampleState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertMultisampleState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUOrigin2D convertOrigin2D(JNIEnv *env, jobject obj) {
    jclass origin2DClass = env->FindClass("android/dawn/Origin2D");

    WGPUOrigin2D converted = {
        .x = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(origin2DClass, "getX", "()I"))),
        .y = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(origin2DClass, "getY", "()I")))
    };

    return converted;
}

const WGPUOrigin2D* convertOrigin2DOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUOrigin2D(convertOrigin2D(env, obj));
}

const WGPUOrigin2D* convertOrigin2DArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUOrigin2D* entries = new WGPUOrigin2D[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertOrigin2D(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUOrigin3D convertOrigin3D(JNIEnv *env, jobject obj) {
    jclass origin3DClass = env->FindClass("android/dawn/Origin3D");

    WGPUOrigin3D converted = {
        .x = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(origin3DClass, "getX", "()I"))),
        .y = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(origin3DClass, "getY", "()I"))),
        .z = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(origin3DClass, "getZ", "()I")))
    };

    return converted;
}

const WGPUOrigin3D* convertOrigin3DOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUOrigin3D(convertOrigin3D(env, obj));
}

const WGPUOrigin3D* convertOrigin3DArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUOrigin3D* entries = new WGPUOrigin3D[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertOrigin3D(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPipelineLayoutDescriptor convertPipelineLayoutDescriptor(JNIEnv *env, jobject obj) {
    jclass pipelineLayoutDescriptorClass = env->FindClass("android/dawn/PipelineLayoutDescriptor");
    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    jmethodID bindGroupLayoutGetHandle = env->GetMethodID(bindGroupLayoutClass, "getHandle", "()J");
    jobjectArray bindGroupLayouts = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(pipelineLayoutDescriptorClass, "getBindGroupLayouts",
                                  "()[Landroid/dawn/BindGroupLayout;")));
    size_t bindGroupLayoutCount = env->GetArrayLength(bindGroupLayouts);
    WGPUBindGroupLayout* nativeBindGroupLayouts = new WGPUBindGroupLayout[bindGroupLayoutCount];
    for (int idx = 0; idx != bindGroupLayoutCount; idx++) {
        nativeBindGroupLayouts[idx] = reinterpret_cast<WGPUBindGroupLayout>(
                env->CallLongMethod(env->GetObjectArrayElement(bindGroupLayouts, idx), bindGroupLayoutGetHandle));
    }

    WGPUPipelineLayoutDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(pipelineLayoutDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .bindGroupLayoutCount =
        bindGroupLayoutCount
,
        .bindGroupLayouts = nativeBindGroupLayouts
    };

    jclass PipelineLayoutPixelLocalStorageClass = env->FindClass("android/dawn/PipelineLayoutPixelLocalStorage");
    if (env->IsInstanceOf(obj, PipelineLayoutPixelLocalStorageClass)) {
         converted.nextInChain = &(new WGPUPipelineLayoutPixelLocalStorage(convertPipelineLayoutPixelLocalStorage(env, obj)))->chain;
    }
    return converted;
}

const WGPUPipelineLayoutDescriptor* convertPipelineLayoutDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPipelineLayoutDescriptor(convertPipelineLayoutDescriptor(env, obj));
}

const WGPUPipelineLayoutDescriptor* convertPipelineLayoutDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPipelineLayoutDescriptor* entries = new WGPUPipelineLayoutDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPipelineLayoutDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPipelineLayoutStorageAttachment convertPipelineLayoutStorageAttachment(JNIEnv *env, jobject obj) {
    jclass pipelineLayoutStorageAttachmentClass = env->FindClass("android/dawn/PipelineLayoutStorageAttachment");

    WGPUPipelineLayoutStorageAttachment converted = {
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(pipelineLayoutStorageAttachmentClass, "getOffset", "()J"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(pipelineLayoutStorageAttachmentClass, "getFormat", "()I")))
    };

    return converted;
}

const WGPUPipelineLayoutStorageAttachment* convertPipelineLayoutStorageAttachmentOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPipelineLayoutStorageAttachment(convertPipelineLayoutStorageAttachment(env, obj));
}

const WGPUPipelineLayoutStorageAttachment* convertPipelineLayoutStorageAttachmentArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPipelineLayoutStorageAttachment* entries = new WGPUPipelineLayoutStorageAttachment[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPipelineLayoutStorageAttachment(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPopErrorScopeCallbackInfo convertPopErrorScopeCallbackInfo(JNIEnv *env, jobject obj) {
    jclass popErrorScopeCallbackInfoClass = env->FindClass("android/dawn/PopErrorScopeCallbackInfo");

    WGPUPopErrorScopeCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(popErrorScopeCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: pop error scope callback */,
        .oldCallback = nullptr /* unknown. annotation: value category: function pointer name: error callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUPopErrorScopeCallbackInfo* convertPopErrorScopeCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPopErrorScopeCallbackInfo(convertPopErrorScopeCallbackInfo(env, obj));
}

const WGPUPopErrorScopeCallbackInfo* convertPopErrorScopeCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPopErrorScopeCallbackInfo* entries = new WGPUPopErrorScopeCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPopErrorScopeCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPrimitiveDepthClipControl convertPrimitiveDepthClipControl(JNIEnv *env, jobject obj) {
    jclass primitiveDepthClipControlClass = env->FindClass("android/dawn/PrimitiveDepthClipControl");

    WGPUPrimitiveDepthClipControl converted = {
        .chain = {.sType = WGPUSType_PrimitiveDepthClipControl},
        .unclippedDepth = env->CallBooleanMethod(obj, env->GetMethodID(primitiveDepthClipControlClass, "getUnclippedDepth", "()Z"))
    };

    return converted;
}

const WGPUPrimitiveDepthClipControl* convertPrimitiveDepthClipControlOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPrimitiveDepthClipControl(convertPrimitiveDepthClipControl(env, obj));
}

const WGPUPrimitiveDepthClipControl* convertPrimitiveDepthClipControlArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPrimitiveDepthClipControl* entries = new WGPUPrimitiveDepthClipControl[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPrimitiveDepthClipControl(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPrimitiveState convertPrimitiveState(JNIEnv *env, jobject obj) {
    jclass primitiveStateClass = env->FindClass("android/dawn/PrimitiveState");

    WGPUPrimitiveState converted = {
        .topology = static_cast<WGPUPrimitiveTopology>(env->CallIntMethod(obj, env->GetMethodID(primitiveStateClass, "getTopology", "()I"))),
        .stripIndexFormat = static_cast<WGPUIndexFormat>(env->CallIntMethod(obj, env->GetMethodID(primitiveStateClass, "getStripIndexFormat", "()I"))),
        .frontFace = static_cast<WGPUFrontFace>(env->CallIntMethod(obj, env->GetMethodID(primitiveStateClass, "getFrontFace", "()I"))),
        .cullMode = static_cast<WGPUCullMode>(env->CallIntMethod(obj, env->GetMethodID(primitiveStateClass, "getCullMode", "()I")))
    };

    jclass PrimitiveDepthClipControlClass = env->FindClass("android/dawn/PrimitiveDepthClipControl");
    if (env->IsInstanceOf(obj, PrimitiveDepthClipControlClass)) {
         converted.nextInChain = &(new WGPUPrimitiveDepthClipControl(convertPrimitiveDepthClipControl(env, obj)))->chain;
    }
    return converted;
}

const WGPUPrimitiveState* convertPrimitiveStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPrimitiveState(convertPrimitiveState(env, obj));
}

const WGPUPrimitiveState* convertPrimitiveStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPrimitiveState* entries = new WGPUPrimitiveState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPrimitiveState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUQuerySetDescriptor convertQuerySetDescriptor(JNIEnv *env, jobject obj) {
    jclass querySetDescriptorClass = env->FindClass("android/dawn/QuerySetDescriptor");

    WGPUQuerySetDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(querySetDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .type = static_cast<WGPUQueryType>(env->CallIntMethod(obj, env->GetMethodID(querySetDescriptorClass, "getType", "()I"))),
        .count = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(querySetDescriptorClass, "getCount", "()I")))
    };

    return converted;
}

const WGPUQuerySetDescriptor* convertQuerySetDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUQuerySetDescriptor(convertQuerySetDescriptor(env, obj));
}

const WGPUQuerySetDescriptor* convertQuerySetDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUQuerySetDescriptor* entries = new WGPUQuerySetDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertQuerySetDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUQueueDescriptor convertQueueDescriptor(JNIEnv *env, jobject obj) {
    jclass queueDescriptorClass = env->FindClass("android/dawn/QueueDescriptor");

    WGPUQueueDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(queueDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPUQueueDescriptor* convertQueueDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUQueueDescriptor(convertQueueDescriptor(env, obj));
}

const WGPUQueueDescriptor* convertQueueDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUQueueDescriptor* entries = new WGPUQueueDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertQueueDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUQueueWorkDoneCallbackInfo convertQueueWorkDoneCallbackInfo(JNIEnv *env, jobject obj) {
    jclass queueWorkDoneCallbackInfoClass = env->FindClass("android/dawn/QueueWorkDoneCallbackInfo");

    WGPUQueueWorkDoneCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(queueWorkDoneCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: queue work done callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUQueueWorkDoneCallbackInfo* convertQueueWorkDoneCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUQueueWorkDoneCallbackInfo(convertQueueWorkDoneCallbackInfo(env, obj));
}

const WGPUQueueWorkDoneCallbackInfo* convertQueueWorkDoneCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUQueueWorkDoneCallbackInfo* entries = new WGPUQueueWorkDoneCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertQueueWorkDoneCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderBundleDescriptor convertRenderBundleDescriptor(JNIEnv *env, jobject obj) {
    jclass renderBundleDescriptorClass = env->FindClass("android/dawn/RenderBundleDescriptor");

    WGPURenderBundleDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(renderBundleDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPURenderBundleDescriptor* convertRenderBundleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderBundleDescriptor(convertRenderBundleDescriptor(env, obj));
}

const WGPURenderBundleDescriptor* convertRenderBundleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderBundleDescriptor* entries = new WGPURenderBundleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderBundleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderBundleEncoderDescriptor convertRenderBundleEncoderDescriptor(JNIEnv *env, jobject obj) {
    jclass renderBundleEncoderDescriptorClass = env->FindClass("android/dawn/RenderBundleEncoderDescriptor");
    jobject nativeColorFormats = env->CallObjectMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getColorFormats","()[I"));

    WGPURenderBundleEncoderDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .colorFormatCount =
        static_cast<size_t>(nativeColorFormats ? env->GetArrayLength(static_cast<jarray>(nativeColorFormats)) : 0)
,
        .colorFormats = reinterpret_cast<WGPUTextureFormat*>(nativeColorFormats),
        .depthStencilFormat = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getDepthStencilFormat", "()I"))),
        .sampleCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getSampleCount", "()I"))),
        .depthReadOnly = env->CallBooleanMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getDepthReadOnly", "()Z")),
        .stencilReadOnly = env->CallBooleanMethod(obj, env->GetMethodID(renderBundleEncoderDescriptorClass, "getStencilReadOnly", "()Z"))
    };

    return converted;
}

const WGPURenderBundleEncoderDescriptor* convertRenderBundleEncoderDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderBundleEncoderDescriptor(convertRenderBundleEncoderDescriptor(env, obj));
}

const WGPURenderBundleEncoderDescriptor* convertRenderBundleEncoderDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderBundleEncoderDescriptor* entries = new WGPURenderBundleEncoderDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderBundleEncoderDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassDepthStencilAttachment convertRenderPassDepthStencilAttachment(JNIEnv *env, jobject obj) {
    jclass renderPassDepthStencilAttachmentClass = env->FindClass("android/dawn/RenderPassDepthStencilAttachment");
    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    jmethodID textureViewGetHandle = env->GetMethodID(textureViewClass, "getHandle", "()J");
    jobject view = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getView",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativeView = view ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(view, textureViewGetHandle)) : nullptr;

    WGPURenderPassDepthStencilAttachment converted = {
        .view = nativeView,
        .depthLoadOp = static_cast<WGPULoadOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getDepthLoadOp", "()I"))),
        .depthStoreOp = static_cast<WGPUStoreOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getDepthStoreOp", "()I"))),
        .depthClearValue = env->CallFloatMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getDepthClearValue", "()F")),
        .depthReadOnly = env->CallBooleanMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getDepthReadOnly", "()Z")),
        .stencilLoadOp = static_cast<WGPULoadOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getStencilLoadOp", "()I"))),
        .stencilStoreOp = static_cast<WGPUStoreOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getStencilStoreOp", "()I"))),
        .stencilClearValue = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getStencilClearValue", "()I"))),
        .stencilReadOnly = env->CallBooleanMethod(obj, env->GetMethodID(renderPassDepthStencilAttachmentClass, "getStencilReadOnly", "()Z"))
    };

    return converted;
}

const WGPURenderPassDepthStencilAttachment* convertRenderPassDepthStencilAttachmentOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassDepthStencilAttachment(convertRenderPassDepthStencilAttachment(env, obj));
}

const WGPURenderPassDepthStencilAttachment* convertRenderPassDepthStencilAttachmentArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassDepthStencilAttachment* entries = new WGPURenderPassDepthStencilAttachment[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassDepthStencilAttachment(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassDescriptorMaxDrawCount convertRenderPassDescriptorMaxDrawCount(JNIEnv *env, jobject obj) {
    jclass renderPassDescriptorMaxDrawCountClass = env->FindClass("android/dawn/RenderPassDescriptorMaxDrawCount");

    WGPURenderPassDescriptorMaxDrawCount converted = {
        .chain = {.sType = WGPUSType_RenderPassDescriptorMaxDrawCount},
        .maxDrawCount = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(renderPassDescriptorMaxDrawCountClass, "getMaxDrawCount", "()J")))
    };

    return converted;
}

const WGPURenderPassDescriptorMaxDrawCount* convertRenderPassDescriptorMaxDrawCountOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassDescriptorMaxDrawCount(convertRenderPassDescriptorMaxDrawCount(env, obj));
}

const WGPURenderPassDescriptorMaxDrawCount* convertRenderPassDescriptorMaxDrawCountArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassDescriptorMaxDrawCount* entries = new WGPURenderPassDescriptorMaxDrawCount[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassDescriptorMaxDrawCount(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassTimestampWrites convertRenderPassTimestampWrites(JNIEnv *env, jobject obj) {
    jclass renderPassTimestampWritesClass = env->FindClass("android/dawn/RenderPassTimestampWrites");
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");
    jobject querySet = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassTimestampWritesClass, "getQuerySet",
                                  "()Landroid/dawn/QuerySet;")));
    WGPUQuerySet nativeQuerySet = querySet ?
        reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, querySetGetHandle)) : nullptr;

    WGPURenderPassTimestampWrites converted = {
        .querySet = nativeQuerySet,
        .beginningOfPassWriteIndex = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(renderPassTimestampWritesClass, "getBeginningOfPassWriteIndex", "()I"))),
        .endOfPassWriteIndex = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(renderPassTimestampWritesClass, "getEndOfPassWriteIndex", "()I")))
    };

    return converted;
}

const WGPURenderPassTimestampWrites* convertRenderPassTimestampWritesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassTimestampWrites(convertRenderPassTimestampWrites(env, obj));
}

const WGPURenderPassTimestampWrites* convertRenderPassTimestampWritesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassTimestampWrites* entries = new WGPURenderPassTimestampWrites[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassTimestampWrites(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURequestAdapterCallbackInfo convertRequestAdapterCallbackInfo(JNIEnv *env, jobject obj) {
    jclass requestAdapterCallbackInfoClass = env->FindClass("android/dawn/RequestAdapterCallbackInfo");

    WGPURequestAdapterCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(requestAdapterCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: request adapter callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPURequestAdapterCallbackInfo* convertRequestAdapterCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURequestAdapterCallbackInfo(convertRequestAdapterCallbackInfo(env, obj));
}

const WGPURequestAdapterCallbackInfo* convertRequestAdapterCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURequestAdapterCallbackInfo* entries = new WGPURequestAdapterCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRequestAdapterCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURequestAdapterOptions convertRequestAdapterOptions(JNIEnv *env, jobject obj) {
    jclass requestAdapterOptionsClass = env->FindClass("android/dawn/RequestAdapterOptions");
    jclass surfaceClass = env->FindClass("android/dawn/Surface");
    jmethodID surfaceGetHandle = env->GetMethodID(surfaceClass, "getHandle", "()J");
    jobject compatibleSurface = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(requestAdapterOptionsClass, "getCompatibleSurface",
                                  "()Landroid/dawn/Surface;")));
    WGPUSurface nativeCompatibleSurface = compatibleSurface ?
        reinterpret_cast<WGPUSurface>(env->CallLongMethod(compatibleSurface, surfaceGetHandle)) : nullptr;

    WGPURequestAdapterOptions converted = {
        .compatibleSurface = nativeCompatibleSurface,
        .powerPreference = static_cast<WGPUPowerPreference>(env->CallIntMethod(obj, env->GetMethodID(requestAdapterOptionsClass, "getPowerPreference", "()I"))),
        .backendType = static_cast<WGPUBackendType>(env->CallIntMethod(obj, env->GetMethodID(requestAdapterOptionsClass, "getBackendType", "()I"))),
        .forceFallbackAdapter = env->CallBooleanMethod(obj, env->GetMethodID(requestAdapterOptionsClass, "getForceFallbackAdapter", "()Z")),
        .compatibilityMode = env->CallBooleanMethod(obj, env->GetMethodID(requestAdapterOptionsClass, "getCompatibilityMode", "()Z"))
    };

    return converted;
}

const WGPURequestAdapterOptions* convertRequestAdapterOptionsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURequestAdapterOptions(convertRequestAdapterOptions(env, obj));
}

const WGPURequestAdapterOptions* convertRequestAdapterOptionsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURequestAdapterOptions* entries = new WGPURequestAdapterOptions[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRequestAdapterOptions(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURequestDeviceCallbackInfo convertRequestDeviceCallbackInfo(JNIEnv *env, jobject obj) {
    jclass requestDeviceCallbackInfoClass = env->FindClass("android/dawn/RequestDeviceCallbackInfo");

    WGPURequestDeviceCallbackInfo converted = {
        .mode = static_cast<WGPUCallbackMode>(env->CallIntMethod(obj, env->GetMethodID(requestDeviceCallbackInfoClass, "getMode", "()I"))),
        .callback = nullptr /* unknown. annotation: value category: function pointer name: request device callback */,
        .userdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPURequestDeviceCallbackInfo* convertRequestDeviceCallbackInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURequestDeviceCallbackInfo(convertRequestDeviceCallbackInfo(env, obj));
}

const WGPURequestDeviceCallbackInfo* convertRequestDeviceCallbackInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURequestDeviceCallbackInfo* entries = new WGPURequestDeviceCallbackInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRequestDeviceCallbackInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSamplerBindingLayout convertSamplerBindingLayout(JNIEnv *env, jobject obj) {
    jclass samplerBindingLayoutClass = env->FindClass("android/dawn/SamplerBindingLayout");

    WGPUSamplerBindingLayout converted = {
        .type = static_cast<WGPUSamplerBindingType>(env->CallIntMethod(obj, env->GetMethodID(samplerBindingLayoutClass, "getType", "()I")))
    };

    return converted;
}

const WGPUSamplerBindingLayout* convertSamplerBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSamplerBindingLayout(convertSamplerBindingLayout(env, obj));
}

const WGPUSamplerBindingLayout* convertSamplerBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSamplerBindingLayout* entries = new WGPUSamplerBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSamplerBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSamplerDescriptor convertSamplerDescriptor(JNIEnv *env, jobject obj) {
    jclass samplerDescriptorClass = env->FindClass("android/dawn/SamplerDescriptor");

    WGPUSamplerDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(samplerDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .addressModeU = static_cast<WGPUAddressMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getAddressModeU", "()I"))),
        .addressModeV = static_cast<WGPUAddressMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getAddressModeV", "()I"))),
        .addressModeW = static_cast<WGPUAddressMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getAddressModeW", "()I"))),
        .magFilter = static_cast<WGPUFilterMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getMagFilter", "()I"))),
        .minFilter = static_cast<WGPUFilterMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getMinFilter", "()I"))),
        .mipmapFilter = static_cast<WGPUMipmapFilterMode>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getMipmapFilter", "()I"))),
        .lodMinClamp = env->CallFloatMethod(obj, env->GetMethodID(samplerDescriptorClass, "getLodMinClamp", "()F")),
        .lodMaxClamp = env->CallFloatMethod(obj, env->GetMethodID(samplerDescriptorClass, "getLodMaxClamp", "()F")),
        .compare = static_cast<WGPUCompareFunction>(env->CallIntMethod(obj, env->GetMethodID(samplerDescriptorClass, "getCompare", "()I"))),
        .maxAnisotropy = static_cast<uint16_t>(env->CallShortMethod(obj, env->GetMethodID(samplerDescriptorClass, "getMaxAnisotropy", "()S")))
    };

    return converted;
}

const WGPUSamplerDescriptor* convertSamplerDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSamplerDescriptor(convertSamplerDescriptor(env, obj));
}

const WGPUSamplerDescriptor* convertSamplerDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSamplerDescriptor* entries = new WGPUSamplerDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSamplerDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUShaderModuleSPIRVDescriptor convertShaderModuleSPIRVDescriptor(JNIEnv *env, jobject obj) {
    jclass shaderModuleSPIRVDescriptorClass = env->FindClass("android/dawn/ShaderModuleSPIRVDescriptor");
    jobject nativeCode = env->CallObjectMethod(obj, env->GetMethodID(shaderModuleSPIRVDescriptorClass, "getCode", "()[I"));

    WGPUShaderModuleSPIRVDescriptor converted = {
        .chain = {.sType = WGPUSType_ShaderModuleSPIRVDescriptor},
        .codeSize =
        static_cast<uint32_t>(nativeCode ? env->GetArrayLength(static_cast<jarray>(nativeCode)) : 0)
,
        .code = nullptr /* unknown. annotation: const* category: native name: uint32_t */

    };

    return converted;
}

const WGPUShaderModuleSPIRVDescriptor* convertShaderModuleSPIRVDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUShaderModuleSPIRVDescriptor(convertShaderModuleSPIRVDescriptor(env, obj));
}

const WGPUShaderModuleSPIRVDescriptor* convertShaderModuleSPIRVDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUShaderModuleSPIRVDescriptor* entries = new WGPUShaderModuleSPIRVDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertShaderModuleSPIRVDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUShaderModuleWGSLDescriptor convertShaderModuleWGSLDescriptor(JNIEnv *env, jobject obj) {
    jclass shaderModuleWGSLDescriptorClass = env->FindClass("android/dawn/ShaderModuleWGSLDescriptor");

    WGPUShaderModuleWGSLDescriptor converted = {
        .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = getString(env, env->CallObjectMethod(obj, env->GetMethodID(shaderModuleWGSLDescriptorClass, "getCode", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPUShaderModuleWGSLDescriptor* convertShaderModuleWGSLDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUShaderModuleWGSLDescriptor(convertShaderModuleWGSLDescriptor(env, obj));
}

const WGPUShaderModuleWGSLDescriptor* convertShaderModuleWGSLDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUShaderModuleWGSLDescriptor* entries = new WGPUShaderModuleWGSLDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertShaderModuleWGSLDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUShaderModuleDescriptor convertShaderModuleDescriptor(JNIEnv *env, jobject obj) {
    jclass shaderModuleDescriptorClass = env->FindClass("android/dawn/ShaderModuleDescriptor");

    WGPUShaderModuleDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(shaderModuleDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    jclass DawnShaderModuleSPIRVOptionsDescriptorClass = env->FindClass("android/dawn/DawnShaderModuleSPIRVOptionsDescriptor");
    if (env->IsInstanceOf(obj, DawnShaderModuleSPIRVOptionsDescriptorClass)) {
         converted.nextInChain = &(new WGPUDawnShaderModuleSPIRVOptionsDescriptor(convertDawnShaderModuleSPIRVOptionsDescriptor(env, obj)))->chain;
    }
    jclass ShaderModuleSPIRVDescriptorClass = env->FindClass("android/dawn/ShaderModuleSPIRVDescriptor");
    if (env->IsInstanceOf(obj, ShaderModuleSPIRVDescriptorClass)) {
         converted.nextInChain = &(new WGPUShaderModuleSPIRVDescriptor(convertShaderModuleSPIRVDescriptor(env, obj)))->chain;
    }
    jclass ShaderModuleWGSLDescriptorClass = env->FindClass("android/dawn/ShaderModuleWGSLDescriptor");
    if (env->IsInstanceOf(obj, ShaderModuleWGSLDescriptorClass)) {
         converted.nextInChain = &(new WGPUShaderModuleWGSLDescriptor(convertShaderModuleWGSLDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUShaderModuleDescriptor* convertShaderModuleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUShaderModuleDescriptor(convertShaderModuleDescriptor(env, obj));
}

const WGPUShaderModuleDescriptor* convertShaderModuleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUShaderModuleDescriptor* entries = new WGPUShaderModuleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertShaderModuleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedBufferMemoryBeginAccessDescriptor convertSharedBufferMemoryBeginAccessDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedBufferMemoryBeginAccessDescriptorClass = env->FindClass("android/dawn/SharedBufferMemoryBeginAccessDescriptor");
    jclass sharedFenceClass = env->FindClass("android/dawn/SharedFence");
    jmethodID sharedFenceGetHandle = env->GetMethodID(sharedFenceClass, "getHandle", "()J");
    jobjectArray fences = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(sharedBufferMemoryBeginAccessDescriptorClass, "getFences",
                                  "()[Landroid/dawn/SharedFence;")));
    size_t fenceCount = env->GetArrayLength(fences);
    WGPUSharedFence* nativeFences = new WGPUSharedFence[fenceCount];
    for (int idx = 0; idx != fenceCount; idx++) {
        nativeFences[idx] = reinterpret_cast<WGPUSharedFence>(
                env->CallLongMethod(env->GetObjectArrayElement(fences, idx), sharedFenceGetHandle));
    }
    jobject nativeSignaledValues = env->CallObjectMethod(obj, env->GetMethodID(sharedBufferMemoryBeginAccessDescriptorClass, "getSignaledValues", "()[J"));

    WGPUSharedBufferMemoryBeginAccessDescriptor converted = {
        .initialized = env->CallBooleanMethod(obj, env->GetMethodID(sharedBufferMemoryBeginAccessDescriptorClass, "getInitialized", "()Z")),
        .fenceCount =
        fenceCount
,
        .fences = nativeFences,
        .fenceCount =
        static_cast<size_t>(nativeSignaledValues ? env->GetArrayLength(static_cast<jarray>(nativeSignaledValues)) : 0)
,
        .signaledValues = nullptr /* unknown. annotation: const* category: native name: uint64_t */

    };

    return converted;
}

const WGPUSharedBufferMemoryBeginAccessDescriptor* convertSharedBufferMemoryBeginAccessDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedBufferMemoryBeginAccessDescriptor(convertSharedBufferMemoryBeginAccessDescriptor(env, obj));
}

const WGPUSharedBufferMemoryBeginAccessDescriptor* convertSharedBufferMemoryBeginAccessDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedBufferMemoryBeginAccessDescriptor* entries = new WGPUSharedBufferMemoryBeginAccessDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedBufferMemoryBeginAccessDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedBufferMemoryDescriptor convertSharedBufferMemoryDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedBufferMemoryDescriptorClass = env->FindClass("android/dawn/SharedBufferMemoryDescriptor");

    WGPUSharedBufferMemoryDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(sharedBufferMemoryDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPUSharedBufferMemoryDescriptor* convertSharedBufferMemoryDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedBufferMemoryDescriptor(convertSharedBufferMemoryDescriptor(env, obj));
}

const WGPUSharedBufferMemoryDescriptor* convertSharedBufferMemoryDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedBufferMemoryDescriptor* entries = new WGPUSharedBufferMemoryDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedBufferMemoryDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedBufferMemoryEndAccessState convertSharedBufferMemoryEndAccessState(JNIEnv *env, jobject obj) {
    jclass sharedBufferMemoryEndAccessStateClass = env->FindClass("android/dawn/SharedBufferMemoryEndAccessState");
    jclass sharedFenceClass = env->FindClass("android/dawn/SharedFence");
    jmethodID sharedFenceGetHandle = env->GetMethodID(sharedFenceClass, "getHandle", "()J");
    jobjectArray fences = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(sharedBufferMemoryEndAccessStateClass, "getFences",
                                  "()[Landroid/dawn/SharedFence;")));
    size_t fenceCount = env->GetArrayLength(fences);
    WGPUSharedFence* nativeFences = new WGPUSharedFence[fenceCount];
    for (int idx = 0; idx != fenceCount; idx++) {
        nativeFences[idx] = reinterpret_cast<WGPUSharedFence>(
                env->CallLongMethod(env->GetObjectArrayElement(fences, idx), sharedFenceGetHandle));
    }
    jobject nativeSignaledValues = env->CallObjectMethod(obj, env->GetMethodID(sharedBufferMemoryEndAccessStateClass, "getSignaledValues", "()[J"));

    WGPUSharedBufferMemoryEndAccessState converted = {
        .initialized = env->CallBooleanMethod(obj, env->GetMethodID(sharedBufferMemoryEndAccessStateClass, "getInitialized", "()Z")),
        .fenceCount =
        fenceCount
,
        .fences = nativeFences,
        .fenceCount =
        static_cast<size_t>(nativeSignaledValues ? env->GetArrayLength(static_cast<jarray>(nativeSignaledValues)) : 0)
,
        .signaledValues = nullptr /* unknown. annotation: const* category: native name: uint64_t */

    };

    return converted;
}

const WGPUSharedBufferMemoryEndAccessState* convertSharedBufferMemoryEndAccessStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedBufferMemoryEndAccessState(convertSharedBufferMemoryEndAccessState(env, obj));
}

const WGPUSharedBufferMemoryEndAccessState* convertSharedBufferMemoryEndAccessStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedBufferMemoryEndAccessState* entries = new WGPUSharedBufferMemoryEndAccessState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedBufferMemoryEndAccessState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedBufferMemoryProperties convertSharedBufferMemoryProperties(JNIEnv *env, jobject obj) {
    jclass sharedBufferMemoryPropertiesClass = env->FindClass("android/dawn/SharedBufferMemoryProperties");

    WGPUSharedBufferMemoryProperties converted = {
        .usage = static_cast<WGPUBufferUsage>(env->CallIntMethod(obj, env->GetMethodID(sharedBufferMemoryPropertiesClass, "getUsage", "()I"))),
        .size = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(sharedBufferMemoryPropertiesClass, "getSize", "()J")))
    };

    return converted;
}

const WGPUSharedBufferMemoryProperties* convertSharedBufferMemoryPropertiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedBufferMemoryProperties(convertSharedBufferMemoryProperties(env, obj));
}

const WGPUSharedBufferMemoryProperties* convertSharedBufferMemoryPropertiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedBufferMemoryProperties* entries = new WGPUSharedBufferMemoryProperties[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedBufferMemoryProperties(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceDXGISharedHandleDescriptor convertSharedFenceDXGISharedHandleDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceDXGISharedHandleDescriptorClass = env->FindClass("android/dawn/SharedFenceDXGISharedHandleDescriptor");

    WGPUSharedFenceDXGISharedHandleDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedFenceDXGISharedHandleDescriptor},
        .handle = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedFenceDXGISharedHandleDescriptor* convertSharedFenceDXGISharedHandleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceDXGISharedHandleDescriptor(convertSharedFenceDXGISharedHandleDescriptor(env, obj));
}

const WGPUSharedFenceDXGISharedHandleDescriptor* convertSharedFenceDXGISharedHandleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceDXGISharedHandleDescriptor* entries = new WGPUSharedFenceDXGISharedHandleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceDXGISharedHandleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceDXGISharedHandleExportInfo convertSharedFenceDXGISharedHandleExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceDXGISharedHandleExportInfoClass = env->FindClass("android/dawn/SharedFenceDXGISharedHandleExportInfo");

    WGPUSharedFenceDXGISharedHandleExportInfo converted = {
        .chain = {.sType = WGPUSType_SharedFenceDXGISharedHandleExportInfo},
        .handle = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedFenceDXGISharedHandleExportInfo* convertSharedFenceDXGISharedHandleExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceDXGISharedHandleExportInfo(convertSharedFenceDXGISharedHandleExportInfo(env, obj));
}

const WGPUSharedFenceDXGISharedHandleExportInfo* convertSharedFenceDXGISharedHandleExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceDXGISharedHandleExportInfo* entries = new WGPUSharedFenceDXGISharedHandleExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceDXGISharedHandleExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceMTLSharedEventDescriptor convertSharedFenceMTLSharedEventDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceMTLSharedEventDescriptorClass = env->FindClass("android/dawn/SharedFenceMTLSharedEventDescriptor");

    WGPUSharedFenceMTLSharedEventDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedFenceMTLSharedEventDescriptor},
        .sharedEvent = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedFenceMTLSharedEventDescriptor* convertSharedFenceMTLSharedEventDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceMTLSharedEventDescriptor(convertSharedFenceMTLSharedEventDescriptor(env, obj));
}

const WGPUSharedFenceMTLSharedEventDescriptor* convertSharedFenceMTLSharedEventDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceMTLSharedEventDescriptor* entries = new WGPUSharedFenceMTLSharedEventDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceMTLSharedEventDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceMTLSharedEventExportInfo convertSharedFenceMTLSharedEventExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceMTLSharedEventExportInfoClass = env->FindClass("android/dawn/SharedFenceMTLSharedEventExportInfo");

    WGPUSharedFenceMTLSharedEventExportInfo converted = {
        .chain = {.sType = WGPUSType_SharedFenceMTLSharedEventExportInfo},
        .sharedEvent = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedFenceMTLSharedEventExportInfo* convertSharedFenceMTLSharedEventExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceMTLSharedEventExportInfo(convertSharedFenceMTLSharedEventExportInfo(env, obj));
}

const WGPUSharedFenceMTLSharedEventExportInfo* convertSharedFenceMTLSharedEventExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceMTLSharedEventExportInfo* entries = new WGPUSharedFenceMTLSharedEventExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceMTLSharedEventExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceDescriptor convertSharedFenceDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceDescriptorClass = env->FindClass("android/dawn/SharedFenceDescriptor");

    WGPUSharedFenceDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(sharedFenceDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    jclass SharedFenceDXGISharedHandleDescriptorClass = env->FindClass("android/dawn/SharedFenceDXGISharedHandleDescriptor");
    if (env->IsInstanceOf(obj, SharedFenceDXGISharedHandleDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedFenceDXGISharedHandleDescriptor(convertSharedFenceDXGISharedHandleDescriptor(env, obj)))->chain;
    }
    jclass SharedFenceMTLSharedEventDescriptorClass = env->FindClass("android/dawn/SharedFenceMTLSharedEventDescriptor");
    if (env->IsInstanceOf(obj, SharedFenceMTLSharedEventDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedFenceMTLSharedEventDescriptor(convertSharedFenceMTLSharedEventDescriptor(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreOpaqueFDDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreOpaqueFDDescriptor");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreOpaqueFDDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor(convertSharedFenceVkSemaphoreOpaqueFDDescriptor(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreSyncFDDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreSyncFDDescriptor");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreSyncFDDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreSyncFDDescriptor(convertSharedFenceVkSemaphoreSyncFDDescriptor(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreZirconHandleDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreZirconHandleDescriptor");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreZirconHandleDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreZirconHandleDescriptor(convertSharedFenceVkSemaphoreZirconHandleDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUSharedFenceDescriptor* convertSharedFenceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceDescriptor(convertSharedFenceDescriptor(env, obj));
}

const WGPUSharedFenceDescriptor* convertSharedFenceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceDescriptor* entries = new WGPUSharedFenceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceExportInfo convertSharedFenceExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceExportInfoClass = env->FindClass("android/dawn/SharedFenceExportInfo");

    WGPUSharedFenceExportInfo converted = {
        .type = static_cast<WGPUSharedFenceType>(env->CallIntMethod(obj, env->GetMethodID(sharedFenceExportInfoClass, "getType", "()I")))
    };

    jclass SharedFenceDXGISharedHandleExportInfoClass = env->FindClass("android/dawn/SharedFenceDXGISharedHandleExportInfo");
    if (env->IsInstanceOf(obj, SharedFenceDXGISharedHandleExportInfoClass)) {
         converted.nextInChain = &(new WGPUSharedFenceDXGISharedHandleExportInfo(convertSharedFenceDXGISharedHandleExportInfo(env, obj)))->chain;
    }
    jclass SharedFenceMTLSharedEventExportInfoClass = env->FindClass("android/dawn/SharedFenceMTLSharedEventExportInfo");
    if (env->IsInstanceOf(obj, SharedFenceMTLSharedEventExportInfoClass)) {
         converted.nextInChain = &(new WGPUSharedFenceMTLSharedEventExportInfo(convertSharedFenceMTLSharedEventExportInfo(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreOpaqueFDExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreOpaqueFDExportInfo");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreOpaqueFDExportInfoClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo(convertSharedFenceVkSemaphoreOpaqueFDExportInfo(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreSyncFDExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreSyncFDExportInfo");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreSyncFDExportInfoClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreSyncFDExportInfo(convertSharedFenceVkSemaphoreSyncFDExportInfo(env, obj)))->chain;
    }
    jclass SharedFenceVkSemaphoreZirconHandleExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreZirconHandleExportInfo");
    if (env->IsInstanceOf(obj, SharedFenceVkSemaphoreZirconHandleExportInfoClass)) {
         converted.nextInChain = &(new WGPUSharedFenceVkSemaphoreZirconHandleExportInfo(convertSharedFenceVkSemaphoreZirconHandleExportInfo(env, obj)))->chain;
    }
    return converted;
}

const WGPUSharedFenceExportInfo* convertSharedFenceExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceExportInfo(convertSharedFenceExportInfo(env, obj));
}

const WGPUSharedFenceExportInfo* convertSharedFenceExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceExportInfo* entries = new WGPUSharedFenceExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor convertSharedFenceVkSemaphoreOpaqueFDDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreOpaqueFDDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreOpaqueFDDescriptor");

    WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreOpaqueFDDescriptor},
        .handle = env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreOpaqueFDDescriptorClass, "getHandle", "()I"))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor* convertSharedFenceVkSemaphoreOpaqueFDDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor(convertSharedFenceVkSemaphoreOpaqueFDDescriptor(env, obj));
}

const WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor* convertSharedFenceVkSemaphoreOpaqueFDDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor* entries = new WGPUSharedFenceVkSemaphoreOpaqueFDDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreOpaqueFDDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo convertSharedFenceVkSemaphoreOpaqueFDExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreOpaqueFDExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreOpaqueFDExportInfo");

    WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreOpaqueFDExportInfo},
        .handle = env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreOpaqueFDExportInfoClass, "getHandle", "()I"))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo* convertSharedFenceVkSemaphoreOpaqueFDExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo(convertSharedFenceVkSemaphoreOpaqueFDExportInfo(env, obj));
}

const WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo* convertSharedFenceVkSemaphoreOpaqueFDExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo* entries = new WGPUSharedFenceVkSemaphoreOpaqueFDExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreOpaqueFDExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreSyncFDDescriptor convertSharedFenceVkSemaphoreSyncFDDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreSyncFDDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreSyncFDDescriptor");

    WGPUSharedFenceVkSemaphoreSyncFDDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreSyncFDDescriptor},
        .handle = env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreSyncFDDescriptorClass, "getHandle", "()I"))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreSyncFDDescriptor* convertSharedFenceVkSemaphoreSyncFDDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreSyncFDDescriptor(convertSharedFenceVkSemaphoreSyncFDDescriptor(env, obj));
}

const WGPUSharedFenceVkSemaphoreSyncFDDescriptor* convertSharedFenceVkSemaphoreSyncFDDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreSyncFDDescriptor* entries = new WGPUSharedFenceVkSemaphoreSyncFDDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreSyncFDDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreSyncFDExportInfo convertSharedFenceVkSemaphoreSyncFDExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreSyncFDExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreSyncFDExportInfo");

    WGPUSharedFenceVkSemaphoreSyncFDExportInfo converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreSyncFDExportInfo},
        .handle = env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreSyncFDExportInfoClass, "getHandle", "()I"))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreSyncFDExportInfo* convertSharedFenceVkSemaphoreSyncFDExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreSyncFDExportInfo(convertSharedFenceVkSemaphoreSyncFDExportInfo(env, obj));
}

const WGPUSharedFenceVkSemaphoreSyncFDExportInfo* convertSharedFenceVkSemaphoreSyncFDExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreSyncFDExportInfo* entries = new WGPUSharedFenceVkSemaphoreSyncFDExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreSyncFDExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreZirconHandleDescriptor convertSharedFenceVkSemaphoreZirconHandleDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreZirconHandleDescriptorClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreZirconHandleDescriptor");

    WGPUSharedFenceVkSemaphoreZirconHandleDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreZirconHandleDescriptor},
        .handle = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreZirconHandleDescriptorClass, "getHandle", "()I")))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreZirconHandleDescriptor* convertSharedFenceVkSemaphoreZirconHandleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreZirconHandleDescriptor(convertSharedFenceVkSemaphoreZirconHandleDescriptor(env, obj));
}

const WGPUSharedFenceVkSemaphoreZirconHandleDescriptor* convertSharedFenceVkSemaphoreZirconHandleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreZirconHandleDescriptor* entries = new WGPUSharedFenceVkSemaphoreZirconHandleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreZirconHandleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedFenceVkSemaphoreZirconHandleExportInfo convertSharedFenceVkSemaphoreZirconHandleExportInfo(JNIEnv *env, jobject obj) {
    jclass sharedFenceVkSemaphoreZirconHandleExportInfoClass = env->FindClass("android/dawn/SharedFenceVkSemaphoreZirconHandleExportInfo");

    WGPUSharedFenceVkSemaphoreZirconHandleExportInfo converted = {
        .chain = {.sType = WGPUSType_SharedFenceVkSemaphoreZirconHandleExportInfo},
        .handle = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedFenceVkSemaphoreZirconHandleExportInfoClass, "getHandle", "()I")))
    };

    return converted;
}

const WGPUSharedFenceVkSemaphoreZirconHandleExportInfo* convertSharedFenceVkSemaphoreZirconHandleExportInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedFenceVkSemaphoreZirconHandleExportInfo(convertSharedFenceVkSemaphoreZirconHandleExportInfo(env, obj));
}

const WGPUSharedFenceVkSemaphoreZirconHandleExportInfo* convertSharedFenceVkSemaphoreZirconHandleExportInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedFenceVkSemaphoreZirconHandleExportInfo* entries = new WGPUSharedFenceVkSemaphoreZirconHandleExportInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedFenceVkSemaphoreZirconHandleExportInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryDXGISharedHandleDescriptor convertSharedTextureMemoryDXGISharedHandleDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryDXGISharedHandleDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryDXGISharedHandleDescriptor");

    WGPUSharedTextureMemoryDXGISharedHandleDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryDXGISharedHandleDescriptor},
        .handle = nullptr /* unknown. annotation: value category: native name: void * */,
        .useKeyedMutex = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryDXGISharedHandleDescriptorClass, "getUseKeyedMutex", "()Z"))
    };

    return converted;
}

const WGPUSharedTextureMemoryDXGISharedHandleDescriptor* convertSharedTextureMemoryDXGISharedHandleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryDXGISharedHandleDescriptor(convertSharedTextureMemoryDXGISharedHandleDescriptor(env, obj));
}

const WGPUSharedTextureMemoryDXGISharedHandleDescriptor* convertSharedTextureMemoryDXGISharedHandleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryDXGISharedHandleDescriptor* entries = new WGPUSharedTextureMemoryDXGISharedHandleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryDXGISharedHandleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryEGLImageDescriptor convertSharedTextureMemoryEGLImageDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryEGLImageDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryEGLImageDescriptor");

    WGPUSharedTextureMemoryEGLImageDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryEGLImageDescriptor},
        .image = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedTextureMemoryEGLImageDescriptor* convertSharedTextureMemoryEGLImageDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryEGLImageDescriptor(convertSharedTextureMemoryEGLImageDescriptor(env, obj));
}

const WGPUSharedTextureMemoryEGLImageDescriptor* convertSharedTextureMemoryEGLImageDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryEGLImageDescriptor* entries = new WGPUSharedTextureMemoryEGLImageDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryEGLImageDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryIOSurfaceDescriptor convertSharedTextureMemoryIOSurfaceDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryIOSurfaceDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryIOSurfaceDescriptor");

    WGPUSharedTextureMemoryIOSurfaceDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryIOSurfaceDescriptor},
        .ioSurface = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedTextureMemoryIOSurfaceDescriptor* convertSharedTextureMemoryIOSurfaceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryIOSurfaceDescriptor(convertSharedTextureMemoryIOSurfaceDescriptor(env, obj));
}

const WGPUSharedTextureMemoryIOSurfaceDescriptor* convertSharedTextureMemoryIOSurfaceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryIOSurfaceDescriptor* entries = new WGPUSharedTextureMemoryIOSurfaceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryIOSurfaceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryAHardwareBufferDescriptor convertSharedTextureMemoryAHardwareBufferDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryAHardwareBufferDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryAHardwareBufferDescriptor");

    WGPUSharedTextureMemoryAHardwareBufferDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryAHardwareBufferDescriptor},
        .handle = nullptr /* unknown. annotation: value category: native name: void * */
    };

    return converted;
}

const WGPUSharedTextureMemoryAHardwareBufferDescriptor* convertSharedTextureMemoryAHardwareBufferDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryAHardwareBufferDescriptor(convertSharedTextureMemoryAHardwareBufferDescriptor(env, obj));
}

const WGPUSharedTextureMemoryAHardwareBufferDescriptor* convertSharedTextureMemoryAHardwareBufferDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryAHardwareBufferDescriptor* entries = new WGPUSharedTextureMemoryAHardwareBufferDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryAHardwareBufferDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryBeginAccessDescriptor convertSharedTextureMemoryBeginAccessDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryBeginAccessDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryBeginAccessDescriptor");
    jclass sharedFenceClass = env->FindClass("android/dawn/SharedFence");
    jmethodID sharedFenceGetHandle = env->GetMethodID(sharedFenceClass, "getHandle", "()J");
    jobjectArray fences = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(sharedTextureMemoryBeginAccessDescriptorClass, "getFences",
                                  "()[Landroid/dawn/SharedFence;")));
    size_t fenceCount = env->GetArrayLength(fences);
    WGPUSharedFence* nativeFences = new WGPUSharedFence[fenceCount];
    for (int idx = 0; idx != fenceCount; idx++) {
        nativeFences[idx] = reinterpret_cast<WGPUSharedFence>(
                env->CallLongMethod(env->GetObjectArrayElement(fences, idx), sharedFenceGetHandle));
    }
    jobject nativeSignaledValues = env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryBeginAccessDescriptorClass, "getSignaledValues", "()[J"));

    WGPUSharedTextureMemoryBeginAccessDescriptor converted = {
        .concurrentRead = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryBeginAccessDescriptorClass, "getConcurrentRead", "()Z")),
        .initialized = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryBeginAccessDescriptorClass, "getInitialized", "()Z")),
        .fenceCount =
        fenceCount
,
        .fences = nativeFences,
        .fenceCount =
        static_cast<size_t>(nativeSignaledValues ? env->GetArrayLength(static_cast<jarray>(nativeSignaledValues)) : 0)
,
        .signaledValues = nullptr /* unknown. annotation: const* category: native name: uint64_t */

    };

    jclass SharedTextureMemoryVkImageLayoutBeginStateClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageLayoutBeginState");
    if (env->IsInstanceOf(obj, SharedTextureMemoryVkImageLayoutBeginStateClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryVkImageLayoutBeginState(convertSharedTextureMemoryVkImageLayoutBeginState(env, obj)))->chain;
    }
    return converted;
}

const WGPUSharedTextureMemoryBeginAccessDescriptor* convertSharedTextureMemoryBeginAccessDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryBeginAccessDescriptor(convertSharedTextureMemoryBeginAccessDescriptor(env, obj));
}

const WGPUSharedTextureMemoryBeginAccessDescriptor* convertSharedTextureMemoryBeginAccessDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryBeginAccessDescriptor* entries = new WGPUSharedTextureMemoryBeginAccessDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryBeginAccessDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryDescriptor convertSharedTextureMemoryDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryDescriptor");

    WGPUSharedTextureMemoryDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    jclass SharedTextureMemoryDXGISharedHandleDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryDXGISharedHandleDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryDXGISharedHandleDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryDXGISharedHandleDescriptor(convertSharedTextureMemoryDXGISharedHandleDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryEGLImageDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryEGLImageDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryEGLImageDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryEGLImageDescriptor(convertSharedTextureMemoryEGLImageDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryIOSurfaceDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryIOSurfaceDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryIOSurfaceDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryIOSurfaceDescriptor(convertSharedTextureMemoryIOSurfaceDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryAHardwareBufferDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryAHardwareBufferDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryAHardwareBufferDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryAHardwareBufferDescriptor(convertSharedTextureMemoryAHardwareBufferDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryOpaqueFDDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryOpaqueFDDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryOpaqueFDDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryOpaqueFDDescriptor(convertSharedTextureMemoryOpaqueFDDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryVkDedicatedAllocationDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryVkDedicatedAllocationDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryVkDedicatedAllocationDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor(convertSharedTextureMemoryVkDedicatedAllocationDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryZirconHandleDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryZirconHandleDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryZirconHandleDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryZirconHandleDescriptor(convertSharedTextureMemoryZirconHandleDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryDmaBufDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryDmaBufDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryDmaBufDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryDmaBufDescriptor(convertSharedTextureMemoryDmaBufDescriptor(env, obj)))->chain;
    }
    jclass SharedTextureMemoryVkImageDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageDescriptor");
    if (env->IsInstanceOf(obj, SharedTextureMemoryVkImageDescriptorClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryVkImageDescriptor(convertSharedTextureMemoryVkImageDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUSharedTextureMemoryDescriptor* convertSharedTextureMemoryDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryDescriptor(convertSharedTextureMemoryDescriptor(env, obj));
}

const WGPUSharedTextureMemoryDescriptor* convertSharedTextureMemoryDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryDescriptor* entries = new WGPUSharedTextureMemoryDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryDmaBufPlane convertSharedTextureMemoryDmaBufPlane(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryDmaBufPlaneClass = env->FindClass("android/dawn/SharedTextureMemoryDmaBufPlane");

    WGPUSharedTextureMemoryDmaBufPlane converted = {
        .fd = env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufPlaneClass, "getFd", "()I")),
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufPlaneClass, "getOffset", "()J"))),
        .stride = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufPlaneClass, "getStride", "()I")))
    };

    return converted;
}

const WGPUSharedTextureMemoryDmaBufPlane* convertSharedTextureMemoryDmaBufPlaneOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryDmaBufPlane(convertSharedTextureMemoryDmaBufPlane(env, obj));
}

const WGPUSharedTextureMemoryDmaBufPlane* convertSharedTextureMemoryDmaBufPlaneArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryDmaBufPlane* entries = new WGPUSharedTextureMemoryDmaBufPlane[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryDmaBufPlane(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryEndAccessState convertSharedTextureMemoryEndAccessState(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryEndAccessStateClass = env->FindClass("android/dawn/SharedTextureMemoryEndAccessState");
    jclass sharedFenceClass = env->FindClass("android/dawn/SharedFence");
    jmethodID sharedFenceGetHandle = env->GetMethodID(sharedFenceClass, "getHandle", "()J");
    jobjectArray fences = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(sharedTextureMemoryEndAccessStateClass, "getFences",
                                  "()[Landroid/dawn/SharedFence;")));
    size_t fenceCount = env->GetArrayLength(fences);
    WGPUSharedFence* nativeFences = new WGPUSharedFence[fenceCount];
    for (int idx = 0; idx != fenceCount; idx++) {
        nativeFences[idx] = reinterpret_cast<WGPUSharedFence>(
                env->CallLongMethod(env->GetObjectArrayElement(fences, idx), sharedFenceGetHandle));
    }
    jobject nativeSignaledValues = env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryEndAccessStateClass, "getSignaledValues", "()[J"));

    WGPUSharedTextureMemoryEndAccessState converted = {
        .initialized = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryEndAccessStateClass, "getInitialized", "()Z")),
        .fenceCount =
        fenceCount
,
        .fences = nativeFences,
        .fenceCount =
        static_cast<size_t>(nativeSignaledValues ? env->GetArrayLength(static_cast<jarray>(nativeSignaledValues)) : 0)
,
        .signaledValues = nullptr /* unknown. annotation: const* category: native name: uint64_t */

    };

    jclass SharedTextureMemoryVkImageLayoutEndStateClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageLayoutEndState");
    if (env->IsInstanceOf(obj, SharedTextureMemoryVkImageLayoutEndStateClass)) {
         converted.nextInChain = &(new WGPUSharedTextureMemoryVkImageLayoutEndState(convertSharedTextureMemoryVkImageLayoutEndState(env, obj)))->chain;
    }
    return converted;
}

const WGPUSharedTextureMemoryEndAccessState* convertSharedTextureMemoryEndAccessStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryEndAccessState(convertSharedTextureMemoryEndAccessState(env, obj));
}

const WGPUSharedTextureMemoryEndAccessState* convertSharedTextureMemoryEndAccessStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryEndAccessState* entries = new WGPUSharedTextureMemoryEndAccessState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryEndAccessState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryOpaqueFDDescriptor convertSharedTextureMemoryOpaqueFDDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryOpaqueFDDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryOpaqueFDDescriptor");

    WGPUSharedTextureMemoryOpaqueFDDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryOpaqueFDDescriptor},
        .vkImageCreateInfo = nullptr /* unknown. annotation: value category: native name: void const * */,
        .memoryFD = env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryOpaqueFDDescriptorClass, "getMemoryFD", "()I")),
        .memoryTypeIndex = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryOpaqueFDDescriptorClass, "getMemoryTypeIndex", "()I"))),
        .allocationSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryOpaqueFDDescriptorClass, "getAllocationSize", "()J"))),
        .dedicatedAllocation = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryOpaqueFDDescriptorClass, "getDedicatedAllocation", "()Z"))
    };

    return converted;
}

const WGPUSharedTextureMemoryOpaqueFDDescriptor* convertSharedTextureMemoryOpaqueFDDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryOpaqueFDDescriptor(convertSharedTextureMemoryOpaqueFDDescriptor(env, obj));
}

const WGPUSharedTextureMemoryOpaqueFDDescriptor* convertSharedTextureMemoryOpaqueFDDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryOpaqueFDDescriptor* entries = new WGPUSharedTextureMemoryOpaqueFDDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryOpaqueFDDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor convertSharedTextureMemoryVkDedicatedAllocationDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryVkDedicatedAllocationDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryVkDedicatedAllocationDescriptor");

    WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryVkDedicatedAllocationDescriptor},
        .dedicatedAllocation = env->CallBooleanMethod(obj, env->GetMethodID(sharedTextureMemoryVkDedicatedAllocationDescriptorClass, "getDedicatedAllocation", "()Z"))
    };

    return converted;
}

const WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor* convertSharedTextureMemoryVkDedicatedAllocationDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor(convertSharedTextureMemoryVkDedicatedAllocationDescriptor(env, obj));
}

const WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor* convertSharedTextureMemoryVkDedicatedAllocationDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor* entries = new WGPUSharedTextureMemoryVkDedicatedAllocationDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryVkDedicatedAllocationDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryVkImageLayoutBeginState convertSharedTextureMemoryVkImageLayoutBeginState(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryVkImageLayoutBeginStateClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageLayoutBeginState");

    WGPUSharedTextureMemoryVkImageLayoutBeginState converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryVkImageLayoutBeginState},
        .oldLayout = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageLayoutBeginStateClass, "getOldLayout", "()I"))),
        .newLayout = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageLayoutBeginStateClass, "getNewLayout", "()I")))
    };

    return converted;
}

const WGPUSharedTextureMemoryVkImageLayoutBeginState* convertSharedTextureMemoryVkImageLayoutBeginStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryVkImageLayoutBeginState(convertSharedTextureMemoryVkImageLayoutBeginState(env, obj));
}

const WGPUSharedTextureMemoryVkImageLayoutBeginState* convertSharedTextureMemoryVkImageLayoutBeginStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryVkImageLayoutBeginState* entries = new WGPUSharedTextureMemoryVkImageLayoutBeginState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryVkImageLayoutBeginState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryVkImageLayoutEndState convertSharedTextureMemoryVkImageLayoutEndState(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryVkImageLayoutEndStateClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageLayoutEndState");

    WGPUSharedTextureMemoryVkImageLayoutEndState converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryVkImageLayoutEndState},
        .oldLayout = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageLayoutEndStateClass, "getOldLayout", "()I"))),
        .newLayout = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageLayoutEndStateClass, "getNewLayout", "()I")))
    };

    return converted;
}

const WGPUSharedTextureMemoryVkImageLayoutEndState* convertSharedTextureMemoryVkImageLayoutEndStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryVkImageLayoutEndState(convertSharedTextureMemoryVkImageLayoutEndState(env, obj));
}

const WGPUSharedTextureMemoryVkImageLayoutEndState* convertSharedTextureMemoryVkImageLayoutEndStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryVkImageLayoutEndState* entries = new WGPUSharedTextureMemoryVkImageLayoutEndState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryVkImageLayoutEndState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryZirconHandleDescriptor convertSharedTextureMemoryZirconHandleDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryZirconHandleDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryZirconHandleDescriptor");

    WGPUSharedTextureMemoryZirconHandleDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryZirconHandleDescriptor},
        .memoryFD = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryZirconHandleDescriptorClass, "getMemoryFD", "()I"))),
        .allocationSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryZirconHandleDescriptorClass, "getAllocationSize", "()J")))
    };

    return converted;
}

const WGPUSharedTextureMemoryZirconHandleDescriptor* convertSharedTextureMemoryZirconHandleDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryZirconHandleDescriptor(convertSharedTextureMemoryZirconHandleDescriptor(env, obj));
}

const WGPUSharedTextureMemoryZirconHandleDescriptor* convertSharedTextureMemoryZirconHandleDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryZirconHandleDescriptor* entries = new WGPUSharedTextureMemoryZirconHandleDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryZirconHandleDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUStaticSamplerBindingLayout convertStaticSamplerBindingLayout(JNIEnv *env, jobject obj) {
    jclass staticSamplerBindingLayoutClass = env->FindClass("android/dawn/StaticSamplerBindingLayout");
    jclass samplerClass = env->FindClass("android/dawn/Sampler");
    jmethodID samplerGetHandle = env->GetMethodID(samplerClass, "getHandle", "()J");
    jobject sampler = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(staticSamplerBindingLayoutClass, "getSampler",
                                  "()Landroid/dawn/Sampler;")));
    WGPUSampler nativeSampler = sampler ?
        reinterpret_cast<WGPUSampler>(env->CallLongMethod(sampler, samplerGetHandle)) : nullptr;

    WGPUStaticSamplerBindingLayout converted = {
        .chain = {.sType = WGPUSType_StaticSamplerBindingLayout},
        .sampler = nativeSampler
    };

    return converted;
}

const WGPUStaticSamplerBindingLayout* convertStaticSamplerBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUStaticSamplerBindingLayout(convertStaticSamplerBindingLayout(env, obj));
}

const WGPUStaticSamplerBindingLayout* convertStaticSamplerBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUStaticSamplerBindingLayout* entries = new WGPUStaticSamplerBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertStaticSamplerBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUStencilFaceState convertStencilFaceState(JNIEnv *env, jobject obj) {
    jclass stencilFaceStateClass = env->FindClass("android/dawn/StencilFaceState");

    WGPUStencilFaceState converted = {
        .compare = static_cast<WGPUCompareFunction>(env->CallIntMethod(obj, env->GetMethodID(stencilFaceStateClass, "getCompare", "()I"))),
        .failOp = static_cast<WGPUStencilOperation>(env->CallIntMethod(obj, env->GetMethodID(stencilFaceStateClass, "getFailOp", "()I"))),
        .depthFailOp = static_cast<WGPUStencilOperation>(env->CallIntMethod(obj, env->GetMethodID(stencilFaceStateClass, "getDepthFailOp", "()I"))),
        .passOp = static_cast<WGPUStencilOperation>(env->CallIntMethod(obj, env->GetMethodID(stencilFaceStateClass, "getPassOp", "()I")))
    };

    return converted;
}

const WGPUStencilFaceState* convertStencilFaceStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUStencilFaceState(convertStencilFaceState(env, obj));
}

const WGPUStencilFaceState* convertStencilFaceStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUStencilFaceState* entries = new WGPUStencilFaceState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertStencilFaceState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUStorageTextureBindingLayout convertStorageTextureBindingLayout(JNIEnv *env, jobject obj) {
    jclass storageTextureBindingLayoutClass = env->FindClass("android/dawn/StorageTextureBindingLayout");

    WGPUStorageTextureBindingLayout converted = {
        .access = static_cast<WGPUStorageTextureAccess>(env->CallIntMethod(obj, env->GetMethodID(storageTextureBindingLayoutClass, "getAccess", "()I"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(storageTextureBindingLayoutClass, "getFormat", "()I"))),
        .viewDimension = static_cast<WGPUTextureViewDimension>(env->CallIntMethod(obj, env->GetMethodID(storageTextureBindingLayoutClass, "getViewDimension", "()I")))
    };

    return converted;
}

const WGPUStorageTextureBindingLayout* convertStorageTextureBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUStorageTextureBindingLayout(convertStorageTextureBindingLayout(env, obj));
}

const WGPUStorageTextureBindingLayout* convertStorageTextureBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUStorageTextureBindingLayout* entries = new WGPUStorageTextureBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertStorageTextureBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptor convertSurfaceDescriptor(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorClass = env->FindClass("android/dawn/SurfaceDescriptor");

    WGPUSurfaceDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(surfaceDescriptorClass, "getLabel", "()Ljava/lang/String;")))
    };

    jclass SurfaceDescriptorFromAndroidNativeWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromAndroidNativeWindow");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromAndroidNativeWindowClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromAndroidNativeWindow(convertSurfaceDescriptorFromAndroidNativeWindow(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromCanvasHTMLSelectorClass = env->FindClass("android/dawn/SurfaceDescriptorFromCanvasHTMLSelector");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromCanvasHTMLSelectorClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromCanvasHTMLSelector(convertSurfaceDescriptorFromCanvasHTMLSelector(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromMetalLayerClass = env->FindClass("android/dawn/SurfaceDescriptorFromMetalLayer");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromMetalLayerClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromMetalLayer(convertSurfaceDescriptorFromMetalLayer(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromWaylandSurfaceClass = env->FindClass("android/dawn/SurfaceDescriptorFromWaylandSurface");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromWaylandSurfaceClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromWaylandSurface(convertSurfaceDescriptorFromWaylandSurface(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromWindowsHWNDClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsHWND");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromWindowsHWNDClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromWindowsHWND(convertSurfaceDescriptorFromWindowsHWND(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromWindowsCoreWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsCoreWindow");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromWindowsCoreWindowClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromWindowsCoreWindow(convertSurfaceDescriptorFromWindowsCoreWindow(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromWindowsSwapChainPanelClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsSwapChainPanel");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromWindowsSwapChainPanelClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromWindowsSwapChainPanel(convertSurfaceDescriptorFromWindowsSwapChainPanel(env, obj)))->chain;
    }
    jclass SurfaceDescriptorFromXlibWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromXlibWindow");
    if (env->IsInstanceOf(obj, SurfaceDescriptorFromXlibWindowClass)) {
         converted.nextInChain = &(new WGPUSurfaceDescriptorFromXlibWindow(convertSurfaceDescriptorFromXlibWindow(env, obj)))->chain;
    }
    return converted;
}

const WGPUSurfaceDescriptor* convertSurfaceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptor(convertSurfaceDescriptor(env, obj));
}

const WGPUSurfaceDescriptor* convertSurfaceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptor* entries = new WGPUSurfaceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromAndroidNativeWindow convertSurfaceDescriptorFromAndroidNativeWindow(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromAndroidNativeWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromAndroidNativeWindow");

    WGPUSurfaceDescriptorFromAndroidNativeWindow converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow},
        .window = reinterpret_cast<void*>(env->CallLongMethod(obj, env->GetMethodID(surfaceDescriptorFromAndroidNativeWindowClass, "getWindow", "()J")))
    };

    return converted;
}

const WGPUSurfaceDescriptorFromAndroidNativeWindow* convertSurfaceDescriptorFromAndroidNativeWindowOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromAndroidNativeWindow(convertSurfaceDescriptorFromAndroidNativeWindow(env, obj));
}

const WGPUSurfaceDescriptorFromAndroidNativeWindow* convertSurfaceDescriptorFromAndroidNativeWindowArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromAndroidNativeWindow* entries = new WGPUSurfaceDescriptorFromAndroidNativeWindow[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromAndroidNativeWindow(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromCanvasHTMLSelector convertSurfaceDescriptorFromCanvasHTMLSelector(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromCanvasHTMLSelectorClass = env->FindClass("android/dawn/SurfaceDescriptorFromCanvasHTMLSelector");

    WGPUSurfaceDescriptorFromCanvasHTMLSelector converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector},
        .selector = getString(env, env->CallObjectMethod(obj, env->GetMethodID(surfaceDescriptorFromCanvasHTMLSelectorClass, "getSelector", "()Ljava/lang/String;")))
    };

    return converted;
}

const WGPUSurfaceDescriptorFromCanvasHTMLSelector* convertSurfaceDescriptorFromCanvasHTMLSelectorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromCanvasHTMLSelector(convertSurfaceDescriptorFromCanvasHTMLSelector(env, obj));
}

const WGPUSurfaceDescriptorFromCanvasHTMLSelector* convertSurfaceDescriptorFromCanvasHTMLSelectorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromCanvasHTMLSelector* entries = new WGPUSurfaceDescriptorFromCanvasHTMLSelector[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromCanvasHTMLSelector(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromMetalLayer convertSurfaceDescriptorFromMetalLayer(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromMetalLayerClass = env->FindClass("android/dawn/SurfaceDescriptorFromMetalLayer");

    WGPUSurfaceDescriptorFromMetalLayer converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromMetalLayer},
        .layer = nullptr /* unknown. annotation: * category: native name: void */
    };

    return converted;
}

const WGPUSurfaceDescriptorFromMetalLayer* convertSurfaceDescriptorFromMetalLayerOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromMetalLayer(convertSurfaceDescriptorFromMetalLayer(env, obj));
}

const WGPUSurfaceDescriptorFromMetalLayer* convertSurfaceDescriptorFromMetalLayerArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromMetalLayer* entries = new WGPUSurfaceDescriptorFromMetalLayer[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromMetalLayer(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromWaylandSurface convertSurfaceDescriptorFromWaylandSurface(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromWaylandSurfaceClass = env->FindClass("android/dawn/SurfaceDescriptorFromWaylandSurface");

    WGPUSurfaceDescriptorFromWaylandSurface converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromWaylandSurface},
        .display = nullptr /* unknown. annotation: * category: native name: void */,
        .surface = nullptr /* unknown. annotation: * category: native name: void */
    };

    return converted;
}

const WGPUSurfaceDescriptorFromWaylandSurface* convertSurfaceDescriptorFromWaylandSurfaceOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromWaylandSurface(convertSurfaceDescriptorFromWaylandSurface(env, obj));
}

const WGPUSurfaceDescriptorFromWaylandSurface* convertSurfaceDescriptorFromWaylandSurfaceArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromWaylandSurface* entries = new WGPUSurfaceDescriptorFromWaylandSurface[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromWaylandSurface(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromWindowsHWND convertSurfaceDescriptorFromWindowsHWND(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromWindowsHWNDClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsHWND");

    WGPUSurfaceDescriptorFromWindowsHWND converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND},
        .hinstance = nullptr /* unknown. annotation: * category: native name: void */,
        .hwnd = nullptr /* unknown. annotation: * category: native name: void */
    };

    return converted;
}

const WGPUSurfaceDescriptorFromWindowsHWND* convertSurfaceDescriptorFromWindowsHWNDOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromWindowsHWND(convertSurfaceDescriptorFromWindowsHWND(env, obj));
}

const WGPUSurfaceDescriptorFromWindowsHWND* convertSurfaceDescriptorFromWindowsHWNDArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromWindowsHWND* entries = new WGPUSurfaceDescriptorFromWindowsHWND[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromWindowsHWND(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromWindowsCoreWindow convertSurfaceDescriptorFromWindowsCoreWindow(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromWindowsCoreWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsCoreWindow");

    WGPUSurfaceDescriptorFromWindowsCoreWindow converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromWindowsCoreWindow},
        .coreWindow = nullptr /* unknown. annotation: * category: native name: void */
    };

    return converted;
}

const WGPUSurfaceDescriptorFromWindowsCoreWindow* convertSurfaceDescriptorFromWindowsCoreWindowOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromWindowsCoreWindow(convertSurfaceDescriptorFromWindowsCoreWindow(env, obj));
}

const WGPUSurfaceDescriptorFromWindowsCoreWindow* convertSurfaceDescriptorFromWindowsCoreWindowArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromWindowsCoreWindow* entries = new WGPUSurfaceDescriptorFromWindowsCoreWindow[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromWindowsCoreWindow(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromWindowsSwapChainPanel convertSurfaceDescriptorFromWindowsSwapChainPanel(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromWindowsSwapChainPanelClass = env->FindClass("android/dawn/SurfaceDescriptorFromWindowsSwapChainPanel");

    WGPUSurfaceDescriptorFromWindowsSwapChainPanel converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromWindowsSwapChainPanel},
        .swapChainPanel = nullptr /* unknown. annotation: * category: native name: void */
    };

    return converted;
}

const WGPUSurfaceDescriptorFromWindowsSwapChainPanel* convertSurfaceDescriptorFromWindowsSwapChainPanelOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromWindowsSwapChainPanel(convertSurfaceDescriptorFromWindowsSwapChainPanel(env, obj));
}

const WGPUSurfaceDescriptorFromWindowsSwapChainPanel* convertSurfaceDescriptorFromWindowsSwapChainPanelArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromWindowsSwapChainPanel* entries = new WGPUSurfaceDescriptorFromWindowsSwapChainPanel[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromWindowsSwapChainPanel(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSurfaceDescriptorFromXlibWindow convertSurfaceDescriptorFromXlibWindow(JNIEnv *env, jobject obj) {
    jclass surfaceDescriptorFromXlibWindowClass = env->FindClass("android/dawn/SurfaceDescriptorFromXlibWindow");

    WGPUSurfaceDescriptorFromXlibWindow converted = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromXlibWindow},
        .display = nullptr /* unknown. annotation: * category: native name: void */,
        .window = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(surfaceDescriptorFromXlibWindowClass, "getWindow", "()J")))
    };

    return converted;
}

const WGPUSurfaceDescriptorFromXlibWindow* convertSurfaceDescriptorFromXlibWindowOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSurfaceDescriptorFromXlibWindow(convertSurfaceDescriptorFromXlibWindow(env, obj));
}

const WGPUSurfaceDescriptorFromXlibWindow* convertSurfaceDescriptorFromXlibWindowArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSurfaceDescriptorFromXlibWindow* entries = new WGPUSurfaceDescriptorFromXlibWindow[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSurfaceDescriptorFromXlibWindow(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSwapChainDescriptor convertSwapChainDescriptor(JNIEnv *env, jobject obj) {
    jclass swapChainDescriptorClass = env->FindClass("android/dawn/SwapChainDescriptor");

    WGPUSwapChainDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .usage = static_cast<WGPUTextureUsage>(env->CallIntMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getUsage", "()I"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getFormat", "()I"))),
        .width = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getWidth", "()I"))),
        .height = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getHeight", "()I"))),
        .presentMode = static_cast<WGPUPresentMode>(env->CallIntMethod(obj, env->GetMethodID(swapChainDescriptorClass, "getPresentMode", "()I")))
    };

    return converted;
}

const WGPUSwapChainDescriptor* convertSwapChainDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSwapChainDescriptor(convertSwapChainDescriptor(env, obj));
}

const WGPUSwapChainDescriptor* convertSwapChainDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSwapChainDescriptor* entries = new WGPUSwapChainDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSwapChainDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUTextureBindingLayout convertTextureBindingLayout(JNIEnv *env, jobject obj) {
    jclass textureBindingLayoutClass = env->FindClass("android/dawn/TextureBindingLayout");

    WGPUTextureBindingLayout converted = {
        .sampleType = static_cast<WGPUTextureSampleType>(env->CallIntMethod(obj, env->GetMethodID(textureBindingLayoutClass, "getSampleType", "()I"))),
        .viewDimension = static_cast<WGPUTextureViewDimension>(env->CallIntMethod(obj, env->GetMethodID(textureBindingLayoutClass, "getViewDimension", "()I"))),
        .multisampled = env->CallBooleanMethod(obj, env->GetMethodID(textureBindingLayoutClass, "getMultisampled", "()Z"))
    };

    return converted;
}

const WGPUTextureBindingLayout* convertTextureBindingLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUTextureBindingLayout(convertTextureBindingLayout(env, obj));
}

const WGPUTextureBindingLayout* convertTextureBindingLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUTextureBindingLayout* entries = new WGPUTextureBindingLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertTextureBindingLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUTextureBindingViewDimensionDescriptor convertTextureBindingViewDimensionDescriptor(JNIEnv *env, jobject obj) {
    jclass textureBindingViewDimensionDescriptorClass = env->FindClass("android/dawn/TextureBindingViewDimensionDescriptor");

    WGPUTextureBindingViewDimensionDescriptor converted = {
        .chain = {.sType = WGPUSType_TextureBindingViewDimensionDescriptor},
        .textureBindingViewDimension = static_cast<WGPUTextureViewDimension>(env->CallIntMethod(obj, env->GetMethodID(textureBindingViewDimensionDescriptorClass, "getTextureBindingViewDimension", "()I")))
    };

    return converted;
}

const WGPUTextureBindingViewDimensionDescriptor* convertTextureBindingViewDimensionDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUTextureBindingViewDimensionDescriptor(convertTextureBindingViewDimensionDescriptor(env, obj));
}

const WGPUTextureBindingViewDimensionDescriptor* convertTextureBindingViewDimensionDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUTextureBindingViewDimensionDescriptor* entries = new WGPUTextureBindingViewDimensionDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertTextureBindingViewDimensionDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUTextureDataLayout convertTextureDataLayout(JNIEnv *env, jobject obj) {
    jclass textureDataLayoutClass = env->FindClass("android/dawn/TextureDataLayout");

    WGPUTextureDataLayout converted = {
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(textureDataLayoutClass, "getOffset", "()J"))),
        .bytesPerRow = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureDataLayoutClass, "getBytesPerRow", "()I"))),
        .rowsPerImage = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureDataLayoutClass, "getRowsPerImage", "()I")))
    };

    return converted;
}

const WGPUTextureDataLayout* convertTextureDataLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUTextureDataLayout(convertTextureDataLayout(env, obj));
}

const WGPUTextureDataLayout* convertTextureDataLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUTextureDataLayout* entries = new WGPUTextureDataLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertTextureDataLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUTextureViewDescriptor convertTextureViewDescriptor(JNIEnv *env, jobject obj) {
    jclass textureViewDescriptorClass = env->FindClass("android/dawn/TextureViewDescriptor");

    WGPUTextureViewDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getFormat", "()I"))),
        .dimension = static_cast<WGPUTextureViewDimension>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getDimension", "()I"))),
        .baseMipLevel = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getBaseMipLevel", "()I"))),
        .mipLevelCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getMipLevelCount", "()I"))),
        .baseArrayLayer = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getBaseArrayLayer", "()I"))),
        .arrayLayerCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getArrayLayerCount", "()I"))),
        .aspect = static_cast<WGPUTextureAspect>(env->CallIntMethod(obj, env->GetMethodID(textureViewDescriptorClass, "getAspect", "()I")))
    };

    return converted;
}

const WGPUTextureViewDescriptor* convertTextureViewDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUTextureViewDescriptor(convertTextureViewDescriptor(env, obj));
}

const WGPUTextureViewDescriptor* convertTextureViewDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUTextureViewDescriptor* entries = new WGPUTextureViewDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertTextureViewDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUVertexAttribute convertVertexAttribute(JNIEnv *env, jobject obj) {
    jclass vertexAttributeClass = env->FindClass("android/dawn/VertexAttribute");

    WGPUVertexAttribute converted = {
        .format = static_cast<WGPUVertexFormat>(env->CallIntMethod(obj, env->GetMethodID(vertexAttributeClass, "getFormat", "()I"))),
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(vertexAttributeClass, "getOffset", "()J"))),
        .shaderLocation = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(vertexAttributeClass, "getShaderLocation", "()I")))
    };

    return converted;
}

const WGPUVertexAttribute* convertVertexAttributeOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUVertexAttribute(convertVertexAttribute(env, obj));
}

const WGPUVertexAttribute* convertVertexAttributeArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUVertexAttribute* entries = new WGPUVertexAttribute[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertVertexAttribute(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUAdapterPropertiesMemoryHeaps convertAdapterPropertiesMemoryHeaps(JNIEnv *env, jobject obj) {
    jclass adapterPropertiesMemoryHeapsClass = env->FindClass("android/dawn/AdapterPropertiesMemoryHeaps");
    jobject nativeHeapInfo = env->CallObjectMethod(obj, env->GetMethodID(adapterPropertiesMemoryHeapsClass, "getHeapInfo", "()[Landroid/dawn/MemoryHeapInfo;"));

    WGPUAdapterPropertiesMemoryHeaps converted = {
        .chain = {.sType = WGPUSType_AdapterPropertiesMemoryHeaps},
        .heapCount =
        static_cast<size_t>(nativeHeapInfo ? env->GetArrayLength(static_cast<jarray>(nativeHeapInfo)) : 0)
,
        .heapInfo = convertMemoryHeapInfoArray(env, static_cast<jobjectArray>(nativeHeapInfo))
    };

    return converted;
}

const WGPUAdapterPropertiesMemoryHeaps* convertAdapterPropertiesMemoryHeapsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUAdapterPropertiesMemoryHeaps(convertAdapterPropertiesMemoryHeaps(env, obj));
}

const WGPUAdapterPropertiesMemoryHeaps* convertAdapterPropertiesMemoryHeapsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUAdapterPropertiesMemoryHeaps* entries = new WGPUAdapterPropertiesMemoryHeaps[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertAdapterPropertiesMemoryHeaps(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBindGroupDescriptor convertBindGroupDescriptor(JNIEnv *env, jobject obj) {
    jclass bindGroupDescriptorClass = env->FindClass("android/dawn/BindGroupDescriptor");
    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    jmethodID bindGroupLayoutGetHandle = env->GetMethodID(bindGroupLayoutClass, "getHandle", "()J");
    jobject layout = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(bindGroupDescriptorClass, "getLayout",
                                  "()Landroid/dawn/BindGroupLayout;")));
    WGPUBindGroupLayout nativeLayout = layout ?
        reinterpret_cast<WGPUBindGroupLayout>(env->CallLongMethod(layout, bindGroupLayoutGetHandle)) : nullptr;
    jobject nativeEntries = env->CallObjectMethod(obj, env->GetMethodID(bindGroupDescriptorClass, "getEntries", "()[Landroid/dawn/BindGroupEntry;"));

    WGPUBindGroupDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .layout = nativeLayout,
        .entryCount =
        static_cast<size_t>(nativeEntries ? env->GetArrayLength(static_cast<jarray>(nativeEntries)) : 0)
,
        .entries = convertBindGroupEntryArray(env, static_cast<jobjectArray>(nativeEntries))
    };

    return converted;
}

const WGPUBindGroupDescriptor* convertBindGroupDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBindGroupDescriptor(convertBindGroupDescriptor(env, obj));
}

const WGPUBindGroupDescriptor* convertBindGroupDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBindGroupDescriptor* entries = new WGPUBindGroupDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBindGroupDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBindGroupLayoutEntry convertBindGroupLayoutEntry(JNIEnv *env, jobject obj) {
    jclass bindGroupLayoutEntryClass = env->FindClass("android/dawn/BindGroupLayoutEntry");

    WGPUBindGroupLayoutEntry converted = {
        .binding = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getBinding", "()I"))),
        .visibility = static_cast<WGPUShaderStage>(env->CallIntMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getVisibility", "()I"))),
        .buffer = convertBufferBindingLayout(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getBuffer", "()Landroid/dawn/BufferBindingLayout;"))),
        .sampler = convertSamplerBindingLayout(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getSampler", "()Landroid/dawn/SamplerBindingLayout;"))),
        .texture = convertTextureBindingLayout(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getTexture", "()Landroid/dawn/TextureBindingLayout;"))),
        .storageTexture = convertStorageTextureBindingLayout(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutEntryClass, "getStorageTexture", "()Landroid/dawn/StorageTextureBindingLayout;")))
    };

    jclass ExternalTextureBindingLayoutClass = env->FindClass("android/dawn/ExternalTextureBindingLayout");
    if (env->IsInstanceOf(obj, ExternalTextureBindingLayoutClass)) {
         converted.nextInChain = &(new WGPUExternalTextureBindingLayout(convertExternalTextureBindingLayout(env, obj)))->chain;
    }
    jclass StaticSamplerBindingLayoutClass = env->FindClass("android/dawn/StaticSamplerBindingLayout");
    if (env->IsInstanceOf(obj, StaticSamplerBindingLayoutClass)) {
         converted.nextInChain = &(new WGPUStaticSamplerBindingLayout(convertStaticSamplerBindingLayout(env, obj)))->chain;
    }
    return converted;
}

const WGPUBindGroupLayoutEntry* convertBindGroupLayoutEntryOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBindGroupLayoutEntry(convertBindGroupLayoutEntry(env, obj));
}

const WGPUBindGroupLayoutEntry* convertBindGroupLayoutEntryArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBindGroupLayoutEntry* entries = new WGPUBindGroupLayoutEntry[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBindGroupLayoutEntry(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBlendState convertBlendState(JNIEnv *env, jobject obj) {
    jclass blendStateClass = env->FindClass("android/dawn/BlendState");

    WGPUBlendState converted = {
        .color = convertBlendComponent(env, env->CallObjectMethod(obj, env->GetMethodID(blendStateClass, "getColor", "()Landroid/dawn/BlendComponent;"))),
        .alpha = convertBlendComponent(env, env->CallObjectMethod(obj, env->GetMethodID(blendStateClass, "getAlpha", "()Landroid/dawn/BlendComponent;")))
    };

    return converted;
}

const WGPUBlendState* convertBlendStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBlendState(convertBlendState(env, obj));
}

const WGPUBlendState* convertBlendStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBlendState* entries = new WGPUBlendState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBlendState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUCompilationInfo convertCompilationInfo(JNIEnv *env, jobject obj) {
    jclass compilationInfoClass = env->FindClass("android/dawn/CompilationInfo");
    jobject nativeMessages = env->CallObjectMethod(obj, env->GetMethodID(compilationInfoClass, "getMessages", "()[Landroid/dawn/CompilationMessage;"));

    WGPUCompilationInfo converted = {
        .messageCount =
        static_cast<size_t>(nativeMessages ? env->GetArrayLength(static_cast<jarray>(nativeMessages)) : 0)
,
        .messages = convertCompilationMessageArray(env, static_cast<jobjectArray>(nativeMessages))
    };

    return converted;
}

const WGPUCompilationInfo* convertCompilationInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUCompilationInfo(convertCompilationInfo(env, obj));
}

const WGPUCompilationInfo* convertCompilationInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUCompilationInfo* entries = new WGPUCompilationInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertCompilationInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUComputePassDescriptor convertComputePassDescriptor(JNIEnv *env, jobject obj) {
    jclass computePassDescriptorClass = env->FindClass("android/dawn/ComputePassDescriptor");

    WGPUComputePassDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(computePassDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .timestampWrites = convertComputePassTimestampWritesOptional(env, env->CallObjectMethod(obj, env->GetMethodID(computePassDescriptorClass, "getTimestampWrites", "()Landroid/dawn/ComputePassTimestampWrites;")))
    };

    return converted;
}

const WGPUComputePassDescriptor* convertComputePassDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUComputePassDescriptor(convertComputePassDescriptor(env, obj));
}

const WGPUComputePassDescriptor* convertComputePassDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUComputePassDescriptor* entries = new WGPUComputePassDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertComputePassDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDepthStencilState convertDepthStencilState(JNIEnv *env, jobject obj) {
    jclass depthStencilStateClass = env->FindClass("android/dawn/DepthStencilState");

    WGPUDepthStencilState converted = {
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(depthStencilStateClass, "getFormat", "()I"))),
        .depthWriteEnabled = env->CallBooleanMethod(obj, env->GetMethodID(depthStencilStateClass, "getDepthWriteEnabled", "()Z")),
        .depthCompare = static_cast<WGPUCompareFunction>(env->CallIntMethod(obj, env->GetMethodID(depthStencilStateClass, "getDepthCompare", "()I"))),
        .stencilFront = convertStencilFaceState(env, env->CallObjectMethod(obj, env->GetMethodID(depthStencilStateClass, "getStencilFront", "()Landroid/dawn/StencilFaceState;"))),
        .stencilBack = convertStencilFaceState(env, env->CallObjectMethod(obj, env->GetMethodID(depthStencilStateClass, "getStencilBack", "()Landroid/dawn/StencilFaceState;"))),
        .stencilReadMask = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(depthStencilStateClass, "getStencilReadMask", "()I"))),
        .stencilWriteMask = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(depthStencilStateClass, "getStencilWriteMask", "()I"))),
        .depthBias = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(depthStencilStateClass, "getDepthBias", "()I"))),
        .depthBiasSlopeScale = env->CallFloatMethod(obj, env->GetMethodID(depthStencilStateClass, "getDepthBiasSlopeScale", "()F")),
        .depthBiasClamp = env->CallFloatMethod(obj, env->GetMethodID(depthStencilStateClass, "getDepthBiasClamp", "()F"))
    };

    jclass DepthStencilStateDepthWriteDefinedDawnClass = env->FindClass("android/dawn/DepthStencilStateDepthWriteDefinedDawn");
    if (env->IsInstanceOf(obj, DepthStencilStateDepthWriteDefinedDawnClass)) {
         converted.nextInChain = &(new WGPUDepthStencilStateDepthWriteDefinedDawn(convertDepthStencilStateDepthWriteDefinedDawn(env, obj)))->chain;
    }
    return converted;
}

const WGPUDepthStencilState* convertDepthStencilStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDepthStencilState(convertDepthStencilState(env, obj));
}

const WGPUDepthStencilState* convertDepthStencilStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDepthStencilState* entries = new WGPUDepthStencilState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDepthStencilState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDrmFormatCapabilities convertDrmFormatCapabilities(JNIEnv *env, jobject obj) {
    jclass drmFormatCapabilitiesClass = env->FindClass("android/dawn/DrmFormatCapabilities");
    jobject nativeProperties = env->CallObjectMethod(obj, env->GetMethodID(drmFormatCapabilitiesClass, "getProperties", "()[Landroid/dawn/DrmFormatProperties;"));

    WGPUDrmFormatCapabilities converted = {
        .chain = {.sType = WGPUSType_DrmFormatCapabilities},
        .propertiesCount =
        static_cast<size_t>(nativeProperties ? env->GetArrayLength(static_cast<jarray>(nativeProperties)) : 0)
,
        .properties = convertDrmFormatPropertiesArray(env, static_cast<jobjectArray>(nativeProperties))
    };

    return converted;
}

const WGPUDrmFormatCapabilities* convertDrmFormatCapabilitiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDrmFormatCapabilities(convertDrmFormatCapabilities(env, obj));
}

const WGPUDrmFormatCapabilities* convertDrmFormatCapabilitiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDrmFormatCapabilities* entries = new WGPUDrmFormatCapabilities[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDrmFormatCapabilities(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUExternalTextureDescriptor convertExternalTextureDescriptor(JNIEnv *env, jobject obj) {
    jclass externalTextureDescriptorClass = env->FindClass("android/dawn/ExternalTextureDescriptor");
    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    jmethodID textureViewGetHandle = env->GetMethodID(textureViewClass, "getHandle", "()J");
    jobject plane0 = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(externalTextureDescriptorClass, "getPlane0",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativePlane0 = plane0 ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(plane0, textureViewGetHandle)) : nullptr;
    jobject plane1 = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(externalTextureDescriptorClass, "getPlane1",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativePlane1 = plane1 ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(plane1, textureViewGetHandle)) : nullptr;
    jobject nativeYuvToRgbConversionMatrix = env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getYuvToRgbConversionMatrix", "()[F"));
    jobject nativeSrcTransferFunctionParameters = env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getSrcTransferFunctionParameters", "()[F"));
    jobject nativeDstTransferFunctionParameters = env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getDstTransferFunctionParameters", "()[F"));
    jobject nativeGamutConversionMatrix = env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getGamutConversionMatrix", "()[F"));

    WGPUExternalTextureDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .plane0 = nativePlane0,
        .plane1 = nativePlane1,
        .visibleOrigin = convertOrigin2D(env, env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getVisibleOrigin", "()Landroid/dawn/Origin2D;"))),
        .visibleSize = convertExtent2D(env, env->CallObjectMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getVisibleSize", "()Landroid/dawn/Extent2D;"))),
        .doYuvToRgbConversionOnly = env->CallBooleanMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getDoYuvToRgbConversionOnly", "()Z")),
        .yuvToRgbConversionMatrix = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeYuvToRgbConversionMatrix), 0),
        .srcTransferFunctionParameters = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeSrcTransferFunctionParameters), 0),
        .dstTransferFunctionParameters = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeDstTransferFunctionParameters), 0),
        .gamutConversionMatrix = env->GetFloatArrayElements(static_cast<jfloatArray>(nativeGamutConversionMatrix), 0),
        .mirrored = env->CallBooleanMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getMirrored", "()Z")),
        .rotation = static_cast<WGPUExternalTextureRotation>(env->CallIntMethod(obj, env->GetMethodID(externalTextureDescriptorClass, "getRotation", "()I")))
    };

    return converted;
}

const WGPUExternalTextureDescriptor* convertExternalTextureDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUExternalTextureDescriptor(convertExternalTextureDescriptor(env, obj));
}

const WGPUExternalTextureDescriptor* convertExternalTextureDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUExternalTextureDescriptor* entries = new WGPUExternalTextureDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertExternalTextureDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUFutureWaitInfo convertFutureWaitInfo(JNIEnv *env, jobject obj) {
    jclass futureWaitInfoClass = env->FindClass("android/dawn/FutureWaitInfo");

    WGPUFutureWaitInfo converted = {
        .future = convertFuture(env, env->CallObjectMethod(obj, env->GetMethodID(futureWaitInfoClass, "getFuture", "()Landroid/dawn/Future;"))),
        .completed = env->CallBooleanMethod(obj, env->GetMethodID(futureWaitInfoClass, "getCompleted", "()Z"))
    };

    return converted;
}

const WGPUFutureWaitInfo* convertFutureWaitInfoOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUFutureWaitInfo(convertFutureWaitInfo(env, obj));
}

const WGPUFutureWaitInfo* convertFutureWaitInfoArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUFutureWaitInfo* entries = new WGPUFutureWaitInfo[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertFutureWaitInfo(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUImageCopyBuffer convertImageCopyBuffer(JNIEnv *env, jobject obj) {
    jclass imageCopyBufferClass = env->FindClass("android/dawn/ImageCopyBuffer");
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");
    jobject buffer = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(imageCopyBufferClass, "getBuffer",
                                  "()Landroid/dawn/Buffer;")));
    WGPUBuffer nativeBuffer = buffer ?
        reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, bufferGetHandle)) : nullptr;

    WGPUImageCopyBuffer converted = {
        .layout = convertTextureDataLayout(env, env->CallObjectMethod(obj, env->GetMethodID(imageCopyBufferClass, "getLayout", "()Landroid/dawn/TextureDataLayout;"))),
        .buffer = nativeBuffer
    };

    return converted;
}

const WGPUImageCopyBuffer* convertImageCopyBufferOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUImageCopyBuffer(convertImageCopyBuffer(env, obj));
}

const WGPUImageCopyBuffer* convertImageCopyBufferArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUImageCopyBuffer* entries = new WGPUImageCopyBuffer[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertImageCopyBuffer(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUImageCopyExternalTexture convertImageCopyExternalTexture(JNIEnv *env, jobject obj) {
    jclass imageCopyExternalTextureClass = env->FindClass("android/dawn/ImageCopyExternalTexture");
    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    jmethodID externalTextureGetHandle = env->GetMethodID(externalTextureClass, "getHandle", "()J");
    jobject externalTexture = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(imageCopyExternalTextureClass, "getExternalTexture",
                                  "()Landroid/dawn/ExternalTexture;")));
    WGPUExternalTexture nativeExternalTexture = externalTexture ?
        reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(externalTexture, externalTextureGetHandle)) : nullptr;

    WGPUImageCopyExternalTexture converted = {
        .externalTexture = nativeExternalTexture,
        .origin = convertOrigin3D(env, env->CallObjectMethod(obj, env->GetMethodID(imageCopyExternalTextureClass, "getOrigin", "()Landroid/dawn/Origin3D;"))),
        .naturalSize = convertExtent2D(env, env->CallObjectMethod(obj, env->GetMethodID(imageCopyExternalTextureClass, "getNaturalSize", "()Landroid/dawn/Extent2D;")))
    };

    return converted;
}

const WGPUImageCopyExternalTexture* convertImageCopyExternalTextureOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUImageCopyExternalTexture(convertImageCopyExternalTexture(env, obj));
}

const WGPUImageCopyExternalTexture* convertImageCopyExternalTextureArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUImageCopyExternalTexture* entries = new WGPUImageCopyExternalTexture[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertImageCopyExternalTexture(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUImageCopyTexture convertImageCopyTexture(JNIEnv *env, jobject obj) {
    jclass imageCopyTextureClass = env->FindClass("android/dawn/ImageCopyTexture");
    jclass textureClass = env->FindClass("android/dawn/Texture");
    jmethodID textureGetHandle = env->GetMethodID(textureClass, "getHandle", "()J");
    jobject texture = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(imageCopyTextureClass, "getTexture",
                                  "()Landroid/dawn/Texture;")));
    WGPUTexture nativeTexture = texture ?
        reinterpret_cast<WGPUTexture>(env->CallLongMethod(texture, textureGetHandle)) : nullptr;

    WGPUImageCopyTexture converted = {
        .texture = nativeTexture,
        .mipLevel = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(imageCopyTextureClass, "getMipLevel", "()I"))),
        .origin = convertOrigin3D(env, env->CallObjectMethod(obj, env->GetMethodID(imageCopyTextureClass, "getOrigin", "()Landroid/dawn/Origin3D;"))),
        .aspect = static_cast<WGPUTextureAspect>(env->CallIntMethod(obj, env->GetMethodID(imageCopyTextureClass, "getAspect", "()I")))
    };

    return converted;
}

const WGPUImageCopyTexture* convertImageCopyTextureOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUImageCopyTexture(convertImageCopyTexture(env, obj));
}

const WGPUImageCopyTexture* convertImageCopyTextureArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUImageCopyTexture* entries = new WGPUImageCopyTexture[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertImageCopyTexture(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUInstanceDescriptor convertInstanceDescriptor(JNIEnv *env, jobject obj) {
    jclass instanceDescriptorClass = env->FindClass("android/dawn/InstanceDescriptor");

    WGPUInstanceDescriptor converted = {
        .features = convertInstanceFeatures(env, env->CallObjectMethod(obj, env->GetMethodID(instanceDescriptorClass, "getFeatures", "()Landroid/dawn/InstanceFeatures;")))
    };

    jclass DawnWireWGSLControlClass = env->FindClass("android/dawn/DawnWireWGSLControl");
    if (env->IsInstanceOf(obj, DawnWireWGSLControlClass)) {
         converted.nextInChain = &(new WGPUDawnWireWGSLControl(convertDawnWireWGSLControl(env, obj)))->chain;
    }
    return converted;
}

const WGPUInstanceDescriptor* convertInstanceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUInstanceDescriptor(convertInstanceDescriptor(env, obj));
}

const WGPUInstanceDescriptor* convertInstanceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUInstanceDescriptor* entries = new WGPUInstanceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertInstanceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUPipelineLayoutPixelLocalStorage convertPipelineLayoutPixelLocalStorage(JNIEnv *env, jobject obj) {
    jclass pipelineLayoutPixelLocalStorageClass = env->FindClass("android/dawn/PipelineLayoutPixelLocalStorage");
    jobject nativeStorageAttachments = env->CallObjectMethod(obj, env->GetMethodID(pipelineLayoutPixelLocalStorageClass, "getStorageAttachments", "()[Landroid/dawn/PipelineLayoutStorageAttachment;"));

    WGPUPipelineLayoutPixelLocalStorage converted = {
        .chain = {.sType = WGPUSType_PipelineLayoutPixelLocalStorage},
        .totalPixelLocalStorageSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(pipelineLayoutPixelLocalStorageClass, "getTotalPixelLocalStorageSize", "()J"))),
        .storageAttachmentCount =
        static_cast<size_t>(nativeStorageAttachments ? env->GetArrayLength(static_cast<jarray>(nativeStorageAttachments)) : 0)
,
        .storageAttachments = convertPipelineLayoutStorageAttachmentArray(env, static_cast<jobjectArray>(nativeStorageAttachments))
    };

    return converted;
}

const WGPUPipelineLayoutPixelLocalStorage* convertPipelineLayoutPixelLocalStorageOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUPipelineLayoutPixelLocalStorage(convertPipelineLayoutPixelLocalStorage(env, obj));
}

const WGPUPipelineLayoutPixelLocalStorage* convertPipelineLayoutPixelLocalStorageArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUPipelineLayoutPixelLocalStorage* entries = new WGPUPipelineLayoutPixelLocalStorage[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertPipelineLayoutPixelLocalStorage(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUProgrammableStageDescriptor convertProgrammableStageDescriptor(JNIEnv *env, jobject obj) {
    jclass programmableStageDescriptorClass = env->FindClass("android/dawn/ProgrammableStageDescriptor");
    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    jmethodID shaderModuleGetHandle = env->GetMethodID(shaderModuleClass, "getHandle", "()J");
    jobject module = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(programmableStageDescriptorClass, "getModule",
                                  "()Landroid/dawn/ShaderModule;")));
    WGPUShaderModule nativeModule = module ?
        reinterpret_cast<WGPUShaderModule>(env->CallLongMethod(module, shaderModuleGetHandle)) : nullptr;
    jobject nativeConstants = env->CallObjectMethod(obj, env->GetMethodID(programmableStageDescriptorClass, "getConstants", "()[Landroid/dawn/ConstantEntry;"));

    WGPUProgrammableStageDescriptor converted = {
        .module = nativeModule,
        .entryPoint = getString(env, env->CallObjectMethod(obj, env->GetMethodID(programmableStageDescriptorClass, "getEntryPoint", "()Ljava/lang/String;"))),
        .constantCount =
        static_cast<size_t>(nativeConstants ? env->GetArrayLength(static_cast<jarray>(nativeConstants)) : 0)
,
        .constants = convertConstantEntryArray(env, static_cast<jobjectArray>(nativeConstants))
    };

    return converted;
}

const WGPUProgrammableStageDescriptor* convertProgrammableStageDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUProgrammableStageDescriptor(convertProgrammableStageDescriptor(env, obj));
}

const WGPUProgrammableStageDescriptor* convertProgrammableStageDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUProgrammableStageDescriptor* entries = new WGPUProgrammableStageDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertProgrammableStageDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassColorAttachment convertRenderPassColorAttachment(JNIEnv *env, jobject obj) {
    jclass renderPassColorAttachmentClass = env->FindClass("android/dawn/RenderPassColorAttachment");
    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    jmethodID textureViewGetHandle = env->GetMethodID(textureViewClass, "getHandle", "()J");
    jobject view = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassColorAttachmentClass, "getView",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativeView = view ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(view, textureViewGetHandle)) : nullptr;
    jobject resolveTarget = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassColorAttachmentClass, "getResolveTarget",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativeResolveTarget = resolveTarget ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(resolveTarget, textureViewGetHandle)) : nullptr;

    WGPURenderPassColorAttachment converted = {
        .view = nativeView,
        .depthSlice = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(renderPassColorAttachmentClass, "getDepthSlice", "()I"))),
        .resolveTarget = nativeResolveTarget,
        .loadOp = static_cast<WGPULoadOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassColorAttachmentClass, "getLoadOp", "()I"))),
        .storeOp = static_cast<WGPUStoreOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassColorAttachmentClass, "getStoreOp", "()I"))),
        .clearValue = convertColor(env, env->CallObjectMethod(obj, env->GetMethodID(renderPassColorAttachmentClass, "getClearValue", "()Landroid/dawn/Color;")))
    };

    jclass DawnRenderPassColorAttachmentRenderToSingleSampledClass = env->FindClass("android/dawn/DawnRenderPassColorAttachmentRenderToSingleSampled");
    if (env->IsInstanceOf(obj, DawnRenderPassColorAttachmentRenderToSingleSampledClass)) {
         converted.nextInChain = &(new WGPUDawnRenderPassColorAttachmentRenderToSingleSampled(convertDawnRenderPassColorAttachmentRenderToSingleSampled(env, obj)))->chain;
    }
    return converted;
}

const WGPURenderPassColorAttachment* convertRenderPassColorAttachmentOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassColorAttachment(convertRenderPassColorAttachment(env, obj));
}

const WGPURenderPassColorAttachment* convertRenderPassColorAttachmentArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassColorAttachment* entries = new WGPURenderPassColorAttachment[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassColorAttachment(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassStorageAttachment convertRenderPassStorageAttachment(JNIEnv *env, jobject obj) {
    jclass renderPassStorageAttachmentClass = env->FindClass("android/dawn/RenderPassStorageAttachment");
    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    jmethodID textureViewGetHandle = env->GetMethodID(textureViewClass, "getHandle", "()J");
    jobject storage = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassStorageAttachmentClass, "getStorage",
                                  "()Landroid/dawn/TextureView;")));
    WGPUTextureView nativeStorage = storage ?
        reinterpret_cast<WGPUTextureView>(env->CallLongMethod(storage, textureViewGetHandle)) : nullptr;

    WGPURenderPassStorageAttachment converted = {
        .offset = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(renderPassStorageAttachmentClass, "getOffset", "()J"))),
        .storage = nativeStorage,
        .loadOp = static_cast<WGPULoadOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassStorageAttachmentClass, "getLoadOp", "()I"))),
        .storeOp = static_cast<WGPUStoreOp>(env->CallIntMethod(obj, env->GetMethodID(renderPassStorageAttachmentClass, "getStoreOp", "()I"))),
        .clearValue = convertColor(env, env->CallObjectMethod(obj, env->GetMethodID(renderPassStorageAttachmentClass, "getClearValue", "()Landroid/dawn/Color;")))
    };

    return converted;
}

const WGPURenderPassStorageAttachment* convertRenderPassStorageAttachmentOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassStorageAttachment(convertRenderPassStorageAttachment(env, obj));
}

const WGPURenderPassStorageAttachment* convertRenderPassStorageAttachmentArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassStorageAttachment* entries = new WGPURenderPassStorageAttachment[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassStorageAttachment(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURequiredLimits convertRequiredLimits(JNIEnv *env, jobject obj) {
    jclass requiredLimitsClass = env->FindClass("android/dawn/RequiredLimits");

    WGPURequiredLimits converted = {
        .limits = convertLimits(env, env->CallObjectMethod(obj, env->GetMethodID(requiredLimitsClass, "getLimits", "()Landroid/dawn/Limits;")))
    };

    return converted;
}

const WGPURequiredLimits* convertRequiredLimitsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURequiredLimits(convertRequiredLimits(env, obj));
}

const WGPURequiredLimits* convertRequiredLimitsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURequiredLimits* entries = new WGPURequiredLimits[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRequiredLimits(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryDmaBufDescriptor convertSharedTextureMemoryDmaBufDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryDmaBufDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryDmaBufDescriptor");
    jobject nativePlanes = env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufDescriptorClass, "getPlanes", "()[Landroid/dawn/SharedTextureMemoryDmaBufPlane;"));

    WGPUSharedTextureMemoryDmaBufDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryDmaBufDescriptor},
        .size = convertExtent3D(env, env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufDescriptorClass, "getSize", "()Landroid/dawn/Extent3D;"))),
        .drmFormat = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufDescriptorClass, "getDrmFormat", "()I"))),
        .drmModifier = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryDmaBufDescriptorClass, "getDrmModifier", "()J"))),
        .planeCount =
        static_cast<size_t>(nativePlanes ? env->GetArrayLength(static_cast<jarray>(nativePlanes)) : 0)
,
        .planes = convertSharedTextureMemoryDmaBufPlaneArray(env, static_cast<jobjectArray>(nativePlanes))
    };

    return converted;
}

const WGPUSharedTextureMemoryDmaBufDescriptor* convertSharedTextureMemoryDmaBufDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryDmaBufDescriptor(convertSharedTextureMemoryDmaBufDescriptor(env, obj));
}

const WGPUSharedTextureMemoryDmaBufDescriptor* convertSharedTextureMemoryDmaBufDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryDmaBufDescriptor* entries = new WGPUSharedTextureMemoryDmaBufDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryDmaBufDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryProperties convertSharedTextureMemoryProperties(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryPropertiesClass = env->FindClass("android/dawn/SharedTextureMemoryProperties");

    WGPUSharedTextureMemoryProperties converted = {
        .usage = static_cast<WGPUTextureUsage>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryPropertiesClass, "getUsage", "()I"))),
        .size = convertExtent3D(env, env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryPropertiesClass, "getSize", "()Landroid/dawn/Extent3D;"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryPropertiesClass, "getFormat", "()I")))
    };

    return converted;
}

const WGPUSharedTextureMemoryProperties* convertSharedTextureMemoryPropertiesOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryProperties(convertSharedTextureMemoryProperties(env, obj));
}

const WGPUSharedTextureMemoryProperties* convertSharedTextureMemoryPropertiesArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryProperties* entries = new WGPUSharedTextureMemoryProperties[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryProperties(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSharedTextureMemoryVkImageDescriptor convertSharedTextureMemoryVkImageDescriptor(JNIEnv *env, jobject obj) {
    jclass sharedTextureMemoryVkImageDescriptorClass = env->FindClass("android/dawn/SharedTextureMemoryVkImageDescriptor");

    WGPUSharedTextureMemoryVkImageDescriptor converted = {
        .chain = {.sType = WGPUSType_SharedTextureMemoryVkImageDescriptor},
        .vkFormat = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageDescriptorClass, "getVkFormat", "()I"))),
        .vkUsageFlags = static_cast<int32_t>(env->CallIntMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageDescriptorClass, "getVkUsageFlags", "()I"))),
        .vkExtent3D = convertExtent3D(env, env->CallObjectMethod(obj, env->GetMethodID(sharedTextureMemoryVkImageDescriptorClass, "getVkExtent3D", "()Landroid/dawn/Extent3D;")))
    };

    return converted;
}

const WGPUSharedTextureMemoryVkImageDescriptor* convertSharedTextureMemoryVkImageDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSharedTextureMemoryVkImageDescriptor(convertSharedTextureMemoryVkImageDescriptor(env, obj));
}

const WGPUSharedTextureMemoryVkImageDescriptor* convertSharedTextureMemoryVkImageDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSharedTextureMemoryVkImageDescriptor* entries = new WGPUSharedTextureMemoryVkImageDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSharedTextureMemoryVkImageDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUSupportedLimits convertSupportedLimits(JNIEnv *env, jobject obj) {
    jclass supportedLimitsClass = env->FindClass("android/dawn/SupportedLimits");

    WGPUSupportedLimits converted = {
        .limits = convertLimits(env, env->CallObjectMethod(obj, env->GetMethodID(supportedLimitsClass, "getLimits", "()Landroid/dawn/Limits;")))
    };

    jclass DawnExperimentalSubgroupLimitsClass = env->FindClass("android/dawn/DawnExperimentalSubgroupLimits");
    if (env->IsInstanceOf(obj, DawnExperimentalSubgroupLimitsClass)) {
         converted.nextInChain = &(new WGPUDawnExperimentalSubgroupLimits(convertDawnExperimentalSubgroupLimits(env, obj)))->chain;
    }
    return converted;
}

const WGPUSupportedLimits* convertSupportedLimitsOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUSupportedLimits(convertSupportedLimits(env, obj));
}

const WGPUSupportedLimits* convertSupportedLimitsArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUSupportedLimits* entries = new WGPUSupportedLimits[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertSupportedLimits(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUTextureDescriptor convertTextureDescriptor(JNIEnv *env, jobject obj) {
    jclass textureDescriptorClass = env->FindClass("android/dawn/TextureDescriptor");
    jobject nativeViewFormats = env->CallObjectMethod(obj, env->GetMethodID(textureDescriptorClass, "getViewFormats","()[I"));

    WGPUTextureDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(textureDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .usage = static_cast<WGPUTextureUsage>(env->CallIntMethod(obj, env->GetMethodID(textureDescriptorClass, "getUsage", "()I"))),
        .dimension = static_cast<WGPUTextureDimension>(env->CallIntMethod(obj, env->GetMethodID(textureDescriptorClass, "getDimension", "()I"))),
        .size = convertExtent3D(env, env->CallObjectMethod(obj, env->GetMethodID(textureDescriptorClass, "getSize", "()Landroid/dawn/Extent3D;"))),
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(textureDescriptorClass, "getFormat", "()I"))),
        .mipLevelCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureDescriptorClass, "getMipLevelCount", "()I"))),
        .sampleCount = static_cast<uint32_t>(env->CallIntMethod(obj, env->GetMethodID(textureDescriptorClass, "getSampleCount", "()I"))),
        .viewFormatCount =
        static_cast<size_t>(nativeViewFormats ? env->GetArrayLength(static_cast<jarray>(nativeViewFormats)) : 0)
,
        .viewFormats = reinterpret_cast<WGPUTextureFormat*>(nativeViewFormats)
    };

    jclass DawnTextureInternalUsageDescriptorClass = env->FindClass("android/dawn/DawnTextureInternalUsageDescriptor");
    if (env->IsInstanceOf(obj, DawnTextureInternalUsageDescriptorClass)) {
         converted.nextInChain = &(new WGPUDawnTextureInternalUsageDescriptor(convertDawnTextureInternalUsageDescriptor(env, obj)))->chain;
    }
    jclass TextureBindingViewDimensionDescriptorClass = env->FindClass("android/dawn/TextureBindingViewDimensionDescriptor");
    if (env->IsInstanceOf(obj, TextureBindingViewDimensionDescriptorClass)) {
         converted.nextInChain = &(new WGPUTextureBindingViewDimensionDescriptor(convertTextureBindingViewDimensionDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUTextureDescriptor* convertTextureDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUTextureDescriptor(convertTextureDescriptor(env, obj));
}

const WGPUTextureDescriptor* convertTextureDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUTextureDescriptor* entries = new WGPUTextureDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertTextureDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUVertexBufferLayout convertVertexBufferLayout(JNIEnv *env, jobject obj) {
    jclass vertexBufferLayoutClass = env->FindClass("android/dawn/VertexBufferLayout");
    jobject nativeAttributes = env->CallObjectMethod(obj, env->GetMethodID(vertexBufferLayoutClass, "getAttributes", "()[Landroid/dawn/VertexAttribute;"));

    WGPUVertexBufferLayout converted = {
        .arrayStride = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(vertexBufferLayoutClass, "getArrayStride", "()J"))),
        .stepMode = static_cast<WGPUVertexStepMode>(env->CallIntMethod(obj, env->GetMethodID(vertexBufferLayoutClass, "getStepMode", "()I"))),
        .attributeCount =
        static_cast<size_t>(nativeAttributes ? env->GetArrayLength(static_cast<jarray>(nativeAttributes)) : 0)
,
        .attributes = convertVertexAttributeArray(env, static_cast<jobjectArray>(nativeAttributes))
    };

    return converted;
}

const WGPUVertexBufferLayout* convertVertexBufferLayoutOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUVertexBufferLayout(convertVertexBufferLayout(env, obj));
}

const WGPUVertexBufferLayout* convertVertexBufferLayoutArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUVertexBufferLayout* entries = new WGPUVertexBufferLayout[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertVertexBufferLayout(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUBindGroupLayoutDescriptor convertBindGroupLayoutDescriptor(JNIEnv *env, jobject obj) {
    jclass bindGroupLayoutDescriptorClass = env->FindClass("android/dawn/BindGroupLayoutDescriptor");
    jobject nativeEntries = env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutDescriptorClass, "getEntries", "()[Landroid/dawn/BindGroupLayoutEntry;"));

    WGPUBindGroupLayoutDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(bindGroupLayoutDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .entryCount =
        static_cast<size_t>(nativeEntries ? env->GetArrayLength(static_cast<jarray>(nativeEntries)) : 0)
,
        .entries = convertBindGroupLayoutEntryArray(env, static_cast<jobjectArray>(nativeEntries))
    };

    return converted;
}

const WGPUBindGroupLayoutDescriptor* convertBindGroupLayoutDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUBindGroupLayoutDescriptor(convertBindGroupLayoutDescriptor(env, obj));
}

const WGPUBindGroupLayoutDescriptor* convertBindGroupLayoutDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUBindGroupLayoutDescriptor* entries = new WGPUBindGroupLayoutDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertBindGroupLayoutDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUColorTargetState convertColorTargetState(JNIEnv *env, jobject obj) {
    jclass colorTargetStateClass = env->FindClass("android/dawn/ColorTargetState");

    WGPUColorTargetState converted = {
        .format = static_cast<WGPUTextureFormat>(env->CallIntMethod(obj, env->GetMethodID(colorTargetStateClass, "getFormat", "()I"))),
        .blend = convertBlendStateOptional(env, env->CallObjectMethod(obj, env->GetMethodID(colorTargetStateClass, "getBlend", "()Landroid/dawn/BlendState;"))),
        .writeMask = static_cast<WGPUColorWriteMask>(env->CallIntMethod(obj, env->GetMethodID(colorTargetStateClass, "getWriteMask", "()I")))
    };

    return converted;
}

const WGPUColorTargetState* convertColorTargetStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUColorTargetState(convertColorTargetState(env, obj));
}

const WGPUColorTargetState* convertColorTargetStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUColorTargetState* entries = new WGPUColorTargetState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertColorTargetState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUComputePipelineDescriptor convertComputePipelineDescriptor(JNIEnv *env, jobject obj) {
    jclass computePipelineDescriptorClass = env->FindClass("android/dawn/ComputePipelineDescriptor");
    jclass pipelineLayoutClass = env->FindClass("android/dawn/PipelineLayout");
    jmethodID pipelineLayoutGetHandle = env->GetMethodID(pipelineLayoutClass, "getHandle", "()J");
    jobject layout = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(computePipelineDescriptorClass, "getLayout",
                                  "()Landroid/dawn/PipelineLayout;")));
    WGPUPipelineLayout nativeLayout = layout ?
        reinterpret_cast<WGPUPipelineLayout>(env->CallLongMethod(layout, pipelineLayoutGetHandle)) : nullptr;

    WGPUComputePipelineDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(computePipelineDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .layout = nativeLayout,
        .compute = convertProgrammableStageDescriptor(env, env->CallObjectMethod(obj, env->GetMethodID(computePipelineDescriptorClass, "getCompute", "()Landroid/dawn/ProgrammableStageDescriptor;")))
    };

    jclass DawnComputePipelineFullSubgroupsClass = env->FindClass("android/dawn/DawnComputePipelineFullSubgroups");
    if (env->IsInstanceOf(obj, DawnComputePipelineFullSubgroupsClass)) {
         converted.nextInChain = &(new WGPUDawnComputePipelineFullSubgroups(convertDawnComputePipelineFullSubgroups(env, obj)))->chain;
    }
    return converted;
}

const WGPUComputePipelineDescriptor* convertComputePipelineDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUComputePipelineDescriptor(convertComputePipelineDescriptor(env, obj));
}

const WGPUComputePipelineDescriptor* convertComputePipelineDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUComputePipelineDescriptor* entries = new WGPUComputePipelineDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertComputePipelineDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUDeviceDescriptor convertDeviceDescriptor(JNIEnv *env, jobject obj) {
    jclass deviceDescriptorClass = env->FindClass("android/dawn/DeviceDescriptor");
    jobject nativeRequiredFeatures = env->CallObjectMethod(obj, env->GetMethodID(deviceDescriptorClass, "getRequiredFeatures","()[I"));

    WGPUDeviceDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(deviceDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .requiredFeatureCount =
        static_cast<size_t>(nativeRequiredFeatures ? env->GetArrayLength(static_cast<jarray>(nativeRequiredFeatures)) : 0)
,
        .requiredFeatures = reinterpret_cast<WGPUFeatureName*>(nativeRequiredFeatures),
        .requiredLimits = convertRequiredLimitsOptional(env, env->CallObjectMethod(obj, env->GetMethodID(deviceDescriptorClass, "getRequiredLimits", "()Landroid/dawn/RequiredLimits;"))),
        .defaultQueue = convertQueueDescriptor(env, env->CallObjectMethod(obj, env->GetMethodID(deviceDescriptorClass, "getDefaultQueue", "()Landroid/dawn/QueueDescriptor;"))),
        .deviceLostCallback = nullptr /* unknown. annotation: value category: function pointer name: device lost callback */,
        .deviceLostUserdata = nullptr /* unknown. annotation: value category: native name: void * */
    };

    jclass DawnCacheDeviceDescriptorClass = env->FindClass("android/dawn/DawnCacheDeviceDescriptor");
    if (env->IsInstanceOf(obj, DawnCacheDeviceDescriptorClass)) {
         converted.nextInChain = &(new WGPUDawnCacheDeviceDescriptor(convertDawnCacheDeviceDescriptor(env, obj)))->chain;
    }
    return converted;
}

const WGPUDeviceDescriptor* convertDeviceDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUDeviceDescriptor(convertDeviceDescriptor(env, obj));
}

const WGPUDeviceDescriptor* convertDeviceDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUDeviceDescriptor* entries = new WGPUDeviceDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertDeviceDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassDescriptor convertRenderPassDescriptor(JNIEnv *env, jobject obj) {
    jclass renderPassDescriptorClass = env->FindClass("android/dawn/RenderPassDescriptor");
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");
    jobject nativeColorAttachments = env->CallObjectMethod(obj, env->GetMethodID(renderPassDescriptorClass, "getColorAttachments", "()[Landroid/dawn/RenderPassColorAttachment;"));
    jobject occlusionQuerySet = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPassDescriptorClass, "getOcclusionQuerySet",
                                  "()Landroid/dawn/QuerySet;")));
    WGPUQuerySet nativeOcclusionQuerySet = occlusionQuerySet ?
        reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(occlusionQuerySet, querySetGetHandle)) : nullptr;

    WGPURenderPassDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(renderPassDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .colorAttachmentCount =
        static_cast<size_t>(nativeColorAttachments ? env->GetArrayLength(static_cast<jarray>(nativeColorAttachments)) : 0)
,
        .colorAttachments = convertRenderPassColorAttachmentArray(env, static_cast<jobjectArray>(nativeColorAttachments)),
        .depthStencilAttachment = convertRenderPassDepthStencilAttachmentOptional(env, env->CallObjectMethod(obj, env->GetMethodID(renderPassDescriptorClass, "getDepthStencilAttachment", "()Landroid/dawn/RenderPassDepthStencilAttachment;"))),
        .occlusionQuerySet = nativeOcclusionQuerySet,
        .timestampWrites = convertRenderPassTimestampWritesOptional(env, env->CallObjectMethod(obj, env->GetMethodID(renderPassDescriptorClass, "getTimestampWrites", "()Landroid/dawn/RenderPassTimestampWrites;")))
    };

    jclass RenderPassDescriptorMaxDrawCountClass = env->FindClass("android/dawn/RenderPassDescriptorMaxDrawCount");
    if (env->IsInstanceOf(obj, RenderPassDescriptorMaxDrawCountClass)) {
         converted.nextInChain = &(new WGPURenderPassDescriptorMaxDrawCount(convertRenderPassDescriptorMaxDrawCount(env, obj)))->chain;
    }
    jclass RenderPassPixelLocalStorageClass = env->FindClass("android/dawn/RenderPassPixelLocalStorage");
    if (env->IsInstanceOf(obj, RenderPassPixelLocalStorageClass)) {
         converted.nextInChain = &(new WGPURenderPassPixelLocalStorage(convertRenderPassPixelLocalStorage(env, obj)))->chain;
    }
    return converted;
}

const WGPURenderPassDescriptor* convertRenderPassDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassDescriptor(convertRenderPassDescriptor(env, obj));
}

const WGPURenderPassDescriptor* convertRenderPassDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassDescriptor* entries = new WGPURenderPassDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPassPixelLocalStorage convertRenderPassPixelLocalStorage(JNIEnv *env, jobject obj) {
    jclass renderPassPixelLocalStorageClass = env->FindClass("android/dawn/RenderPassPixelLocalStorage");
    jobject nativeStorageAttachments = env->CallObjectMethod(obj, env->GetMethodID(renderPassPixelLocalStorageClass, "getStorageAttachments", "()[Landroid/dawn/RenderPassStorageAttachment;"));

    WGPURenderPassPixelLocalStorage converted = {
        .chain = {.sType = WGPUSType_RenderPassPixelLocalStorage},
        .totalPixelLocalStorageSize = static_cast<uint64_t>(env->CallLongMethod(obj, env->GetMethodID(renderPassPixelLocalStorageClass, "getTotalPixelLocalStorageSize", "()J"))),
        .storageAttachmentCount =
        static_cast<size_t>(nativeStorageAttachments ? env->GetArrayLength(static_cast<jarray>(nativeStorageAttachments)) : 0)
,
        .storageAttachments = convertRenderPassStorageAttachmentArray(env, static_cast<jobjectArray>(nativeStorageAttachments))
    };

    return converted;
}

const WGPURenderPassPixelLocalStorage* convertRenderPassPixelLocalStorageOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPassPixelLocalStorage(convertRenderPassPixelLocalStorage(env, obj));
}

const WGPURenderPassPixelLocalStorage* convertRenderPassPixelLocalStorageArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPassPixelLocalStorage* entries = new WGPURenderPassPixelLocalStorage[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPassPixelLocalStorage(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUVertexState convertVertexState(JNIEnv *env, jobject obj) {
    jclass vertexStateClass = env->FindClass("android/dawn/VertexState");
    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    jmethodID shaderModuleGetHandle = env->GetMethodID(shaderModuleClass, "getHandle", "()J");
    jobject module = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(vertexStateClass, "getModule",
                                  "()Landroid/dawn/ShaderModule;")));
    WGPUShaderModule nativeModule = module ?
        reinterpret_cast<WGPUShaderModule>(env->CallLongMethod(module, shaderModuleGetHandle)) : nullptr;
    jobject nativeConstants = env->CallObjectMethod(obj, env->GetMethodID(vertexStateClass, "getConstants", "()[Landroid/dawn/ConstantEntry;"));
    jobject nativeBuffers = env->CallObjectMethod(obj, env->GetMethodID(vertexStateClass, "getBuffers", "()[Landroid/dawn/VertexBufferLayout;"));

    WGPUVertexState converted = {
        .module = nativeModule,
        .entryPoint = getString(env, env->CallObjectMethod(obj, env->GetMethodID(vertexStateClass, "getEntryPoint", "()Ljava/lang/String;"))),
        .constantCount =
        static_cast<size_t>(nativeConstants ? env->GetArrayLength(static_cast<jarray>(nativeConstants)) : 0)
,
        .constants = convertConstantEntryArray(env, static_cast<jobjectArray>(nativeConstants)),
        .bufferCount =
        static_cast<size_t>(nativeBuffers ? env->GetArrayLength(static_cast<jarray>(nativeBuffers)) : 0)
,
        .buffers = convertVertexBufferLayoutArray(env, static_cast<jobjectArray>(nativeBuffers))
    };

    return converted;
}

const WGPUVertexState* convertVertexStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUVertexState(convertVertexState(env, obj));
}

const WGPUVertexState* convertVertexStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUVertexState* entries = new WGPUVertexState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertVertexState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPUFragmentState convertFragmentState(JNIEnv *env, jobject obj) {
    jclass fragmentStateClass = env->FindClass("android/dawn/FragmentState");
    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    jmethodID shaderModuleGetHandle = env->GetMethodID(shaderModuleClass, "getHandle", "()J");
    jobject module = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(fragmentStateClass, "getModule",
                                  "()Landroid/dawn/ShaderModule;")));
    WGPUShaderModule nativeModule = module ?
        reinterpret_cast<WGPUShaderModule>(env->CallLongMethod(module, shaderModuleGetHandle)) : nullptr;
    jobject nativeConstants = env->CallObjectMethod(obj, env->GetMethodID(fragmentStateClass, "getConstants", "()[Landroid/dawn/ConstantEntry;"));
    jobject nativeTargets = env->CallObjectMethod(obj, env->GetMethodID(fragmentStateClass, "getTargets", "()[Landroid/dawn/ColorTargetState;"));

    WGPUFragmentState converted = {
        .module = nativeModule,
        .entryPoint = getString(env, env->CallObjectMethod(obj, env->GetMethodID(fragmentStateClass, "getEntryPoint", "()Ljava/lang/String;"))),
        .constantCount =
        static_cast<size_t>(nativeConstants ? env->GetArrayLength(static_cast<jarray>(nativeConstants)) : 0)
,
        .constants = convertConstantEntryArray(env, static_cast<jobjectArray>(nativeConstants)),
        .targetCount =
        static_cast<size_t>(nativeTargets ? env->GetArrayLength(static_cast<jarray>(nativeTargets)) : 0)
,
        .targets = convertColorTargetStateArray(env, static_cast<jobjectArray>(nativeTargets))
    };

    return converted;
}

const WGPUFragmentState* convertFragmentStateOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPUFragmentState(convertFragmentState(env, obj));
}

const WGPUFragmentState* convertFragmentStateArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPUFragmentState* entries = new WGPUFragmentState[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertFragmentState(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}

const WGPURenderPipelineDescriptor convertRenderPipelineDescriptor(JNIEnv *env, jobject obj) {
    jclass renderPipelineDescriptorClass = env->FindClass("android/dawn/RenderPipelineDescriptor");
    jclass pipelineLayoutClass = env->FindClass("android/dawn/PipelineLayout");
    jmethodID pipelineLayoutGetHandle = env->GetMethodID(pipelineLayoutClass, "getHandle", "()J");
    jobject layout = static_cast<jobjectArray>(env->CallObjectMethod(
            obj, env->GetMethodID(renderPipelineDescriptorClass, "getLayout",
                                  "()Landroid/dawn/PipelineLayout;")));
    WGPUPipelineLayout nativeLayout = layout ?
        reinterpret_cast<WGPUPipelineLayout>(env->CallLongMethod(layout, pipelineLayoutGetHandle)) : nullptr;

    WGPURenderPipelineDescriptor converted = {
        .label = getString(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getLabel", "()Ljava/lang/String;"))),
        .layout = nativeLayout,
        .vertex = convertVertexState(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getVertex", "()Landroid/dawn/VertexState;"))),
        .primitive = convertPrimitiveState(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getPrimitive", "()Landroid/dawn/PrimitiveState;"))),
        .depthStencil = convertDepthStencilStateOptional(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getDepthStencil", "()Landroid/dawn/DepthStencilState;"))),
        .multisample = convertMultisampleState(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getMultisample", "()Landroid/dawn/MultisampleState;"))),
        .fragment = convertFragmentStateOptional(env, env->CallObjectMethod(obj, env->GetMethodID(renderPipelineDescriptorClass, "getFragment", "()Landroid/dawn/FragmentState;")))
    };

    return converted;
}

const WGPURenderPipelineDescriptor* convertRenderPipelineDescriptorOptional(JNIEnv *env, jobject obj) {
    if (!obj) {
        return nullptr;
    }
    return new WGPURenderPipelineDescriptor(convertRenderPipelineDescriptor(env, obj));
}

const WGPURenderPipelineDescriptor* convertRenderPipelineDescriptorArray(JNIEnv *env, jobjectArray arr) {
    if (!arr) {
        return nullptr;
    }
    jsize len = env->GetArrayLength(arr);
    WGPURenderPipelineDescriptor* entries = new WGPURenderPipelineDescriptor[len];
    for (int idx = 0; idx != len; idx++) {
        entries[idx] = convertRenderPipelineDescriptor(env, env->GetObjectArrayElement(arr, idx));
    }
    return entries;
}


DEFAULT extern "C" jobject Java_android_dawn_Adapter_createDevice(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    const WGPUAdapter handle = reinterpret_cast<WGPUAdapter>(env->CallLongMethod(obj, env->GetMethodID(adapterClass, "getHandle", "()J")));

    const WGPUDeviceDescriptor* nativeDescriptor = descriptor ? new WGPUDeviceDescriptor(convertDeviceDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuAdapterCreateDevice(handle
, nativeDescriptor
);

    jclass deviceClass = env->FindClass("android/dawn/Device");
    return env->NewObject(deviceClass, env->GetMethodID(deviceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Adapter_getInstance(
       JNIEnv *env , jobject obj) {

    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    const WGPUAdapter handle = reinterpret_cast<WGPUAdapter>(env->CallLongMethod(obj, env->GetMethodID(adapterClass, "getHandle", "()J")));


    auto result = wgpuAdapterGetInstance(handle
);

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    return env->NewObject(instanceClass, env->GetMethodID(instanceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jboolean Java_android_dawn_Adapter_hasFeature(
       JNIEnv *env , jobject obj,
    jint
     feature) {

    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    const WGPUAdapter handle = reinterpret_cast<WGPUAdapter>(env->CallLongMethod(obj, env->GetMethodID(adapterClass, "getHandle", "()J")));


    auto result = wgpuAdapterHasFeature(handle
, (WGPUFeatureName) feature
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_Adapter_requestDevice(
       JNIEnv *env , jobject obj,
    jobject
     descriptor,
    jobject
     callback) {

    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    const WGPUAdapter handle = reinterpret_cast<WGPUAdapter>(env->CallLongMethod(obj, env->GetMethodID(adapterClass, "getHandle", "()J")));

    const WGPUDeviceDescriptor* nativeDescriptor = descriptor ? new WGPUDeviceDescriptor(convertDeviceDescriptor(env, descriptor)) : nullptr;
    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPURequestDeviceCallback nativeCallback = [](
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass requestDeviceCallbackClass = env->FindClass("android/dawn/RequestDeviceCallback");
    jclass deviceClass = env->FindClass("android/dawn/Device");
    jmethodID id = env->GetMethodID(requestDeviceCallbackClass, "callback", "(ILandroid/dawn/Device;Ljava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
,
    env->NewObject(deviceClass, env->GetMethodID(deviceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(device))
,
    env->NewStringUTF(message)
);
    };

    wgpuAdapterRequestDevice(handle
, nativeDescriptor
,nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_BindGroup_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass bindGroupClass = env->FindClass("android/dawn/BindGroup");
    const WGPUBindGroup handle = reinterpret_cast<WGPUBindGroup>(env->CallLongMethod(obj, env->GetMethodID(bindGroupClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuBindGroupSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_BindGroupLayout_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    const WGPUBindGroupLayout handle = reinterpret_cast<WGPUBindGroupLayout>(env->CallLongMethod(obj, env->GetMethodID(bindGroupLayoutClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuBindGroupLayoutSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Buffer_destroy(
       JNIEnv *env , jobject obj) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));


    wgpuBufferDestroy(handle
);

}

DEFAULT extern "C" jint Java_android_dawn_Buffer_getMapState(
       JNIEnv *env , jobject obj) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));


    auto result = wgpuBufferGetMapState(handle
);

    return result;
}

DEFAULT extern "C" jlong Java_android_dawn_Buffer_getSize(
       JNIEnv *env , jobject obj) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));


    auto result = wgpuBufferGetSize(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Buffer_getUsage(
       JNIEnv *env , jobject obj) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));


    auto result = wgpuBufferGetUsage(handle
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_Buffer_mapAsync(
       JNIEnv *env , jobject obj,
    jint
     mode,
    jlong
     offset,
    jlong
     size,
    jobject
     callback) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUBufferMapCallback nativeCallback = [](
    WGPUBufferMapAsyncStatus status,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass bufferMapCallbackClass = env->FindClass("android/dawn/BufferMapCallback");
    jmethodID id = env->GetMethodID(bufferMapCallbackClass, "callback", "(I)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
);
    };

    wgpuBufferMapAsync(handle
, (WGPUMapMode) mode
,offset
,size
,nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_Buffer_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuBufferSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Buffer_unmap(
       JNIEnv *env , jobject obj) {

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    const WGPUBuffer handle = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(obj, env->GetMethodID(bufferClass, "getHandle", "()J")));


    wgpuBufferUnmap(handle
);

}

DEFAULT extern "C" void Java_android_dawn_CommandBuffer_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass commandBufferClass = env->FindClass("android/dawn/CommandBuffer");
    const WGPUCommandBuffer handle = reinterpret_cast<WGPUCommandBuffer>(env->CallLongMethod(obj, env->GetMethodID(commandBufferClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuCommandBufferSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jobject Java_android_dawn_CommandEncoder_beginComputePass(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPUComputePassDescriptor* nativeDescriptor = descriptor ? new WGPUComputePassDescriptor(convertComputePassDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuCommandEncoderBeginComputePass(handle
, nativeDescriptor
);

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    return env->NewObject(computePassEncoderClass, env->GetMethodID(computePassEncoderClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_CommandEncoder_beginRenderPass(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPURenderPassDescriptor* nativeDescriptor = descriptor ? new WGPURenderPassDescriptor(convertRenderPassDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuCommandEncoderBeginRenderPass(handle
, nativeDescriptor
);

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    return env->NewObject(renderPassEncoderClass, env->GetMethodID(renderPassEncoderClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_clearBuffer(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jlong
     offset,
    jlong
     size) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuCommandEncoderClearBuffer(handle
, nativeBuffer
,offset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_copyBufferToBuffer(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jlong
     sourceOffset,
    jobject
     destination,
    jlong
     destinationOffset,
    jlong
     size) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeSource = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(source, env->GetMethodID(bufferClass, "getHandle", "()J")));    const WGPUBuffer nativeDestination = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(destination, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuCommandEncoderCopyBufferToBuffer(handle
, nativeSource
,sourceOffset
,nativeDestination
,destinationOffset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_copyBufferToTexture(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jobject
     destination,
    jobject
     copySize) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPUImageCopyBuffer* nativeSource = source ? new WGPUImageCopyBuffer(convertImageCopyBuffer(env, source)) : nullptr;
    const WGPUImageCopyTexture* nativeDestination = destination ? new WGPUImageCopyTexture(convertImageCopyTexture(env, destination)) : nullptr;
    const WGPUExtent3D* nativeCopySize = copySize ? new WGPUExtent3D(convertExtent3D(env, copySize)) : nullptr;

    wgpuCommandEncoderCopyBufferToTexture(handle
, nativeSource
,nativeDestination
,nativeCopySize
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_copyTextureToBuffer(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jobject
     destination,
    jobject
     copySize) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPUImageCopyTexture* nativeSource = source ? new WGPUImageCopyTexture(convertImageCopyTexture(env, source)) : nullptr;
    const WGPUImageCopyBuffer* nativeDestination = destination ? new WGPUImageCopyBuffer(convertImageCopyBuffer(env, destination)) : nullptr;
    const WGPUExtent3D* nativeCopySize = copySize ? new WGPUExtent3D(convertExtent3D(env, copySize)) : nullptr;

    wgpuCommandEncoderCopyTextureToBuffer(handle
, nativeSource
,nativeDestination
,nativeCopySize
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_copyTextureToTexture(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jobject
     destination,
    jobject
     copySize) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPUImageCopyTexture* nativeSource = source ? new WGPUImageCopyTexture(convertImageCopyTexture(env, source)) : nullptr;
    const WGPUImageCopyTexture* nativeDestination = destination ? new WGPUImageCopyTexture(convertImageCopyTexture(env, destination)) : nullptr;
    const WGPUExtent3D* nativeCopySize = copySize ? new WGPUExtent3D(convertExtent3D(env, copySize)) : nullptr;

    wgpuCommandEncoderCopyTextureToTexture(handle
, nativeSource
,nativeDestination
,nativeCopySize
);

}

DEFAULT extern "C" jobject Java_android_dawn_CommandEncoder_finish(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const WGPUCommandBufferDescriptor* nativeDescriptor = descriptor ? new WGPUCommandBufferDescriptor(convertCommandBufferDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuCommandEncoderFinish(handle
, nativeDescriptor
);

    jclass commandBufferClass = env->FindClass("android/dawn/CommandBuffer");
    return env->NewObject(commandBufferClass, env->GetMethodID(commandBufferClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_injectValidationError(
       JNIEnv *env , jobject obj,
    jstring
     message) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const char *nativeMessage = getString(env, message);

    wgpuCommandEncoderInjectValidationError(handle
, nativeMessage
);

    env->ReleaseStringUTFChars(message, nativeMessage);
}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_insertDebugMarker(
       JNIEnv *env , jobject obj,
    jstring
     markerLabel) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const char *nativeMarkerLabel = getString(env, markerLabel);

    wgpuCommandEncoderInsertDebugMarker(handle
, nativeMarkerLabel
);

    env->ReleaseStringUTFChars(markerLabel, nativeMarkerLabel);
}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_popDebugGroup(
       JNIEnv *env , jobject obj) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));


    wgpuCommandEncoderPopDebugGroup(handle
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_pushDebugGroup(
       JNIEnv *env , jobject obj,
    jstring
     groupLabel) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const char *nativeGroupLabel = getString(env, groupLabel);

    wgpuCommandEncoderPushDebugGroup(handle
, nativeGroupLabel
);

    env->ReleaseStringUTFChars(groupLabel, nativeGroupLabel);
}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_resolveQuerySet(
       JNIEnv *env , jobject obj,
    jobject
     querySet,
    jint
     firstQuery,
    jint
     queryCount,
    jobject
     destination,
    jlong
     destinationOffset) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUQuerySet nativeQuerySet = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, env->GetMethodID(querySetClass, "getHandle", "()J")));    const WGPUBuffer nativeDestination = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(destination, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuCommandEncoderResolveQuerySet(handle
, nativeQuerySet
,firstQuery
,queryCount
,nativeDestination
,destinationOffset
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuCommandEncoderSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_writeBuffer(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jlong
     bufferOffset,
    jbyteArray
     data,
    jlong
     size) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));    const uint8_t *nativeData = data ? reinterpret_cast<const uint8_t *>(env->GetByteArrayElements(data, 0)) : nullptr;

    wgpuCommandEncoderWriteBuffer(handle
, nativeBuffer
,bufferOffset
,nativeData
,size
);

}

DEFAULT extern "C" void Java_android_dawn_CommandEncoder_writeTimestamp(
       JNIEnv *env , jobject obj,
    jobject
     querySet,
    jint
     queryIndex) {

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    const WGPUCommandEncoder handle = reinterpret_cast<WGPUCommandEncoder>(env->CallLongMethod(obj, env->GetMethodID(commandEncoderClass, "getHandle", "()J")));
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");

    const WGPUQuerySet nativeQuerySet = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, env->GetMethodID(querySetClass, "getHandle", "()J")));
    wgpuCommandEncoderWriteTimestamp(handle
, nativeQuerySet
,queryIndex
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_dispatchWorkgroups(
       JNIEnv *env , jobject obj,
    jint
     workgroupCountX,
    jint
     workgroupCountY,
    jint
     workgroupCountZ) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));


    wgpuComputePassEncoderDispatchWorkgroups(handle
, workgroupCountX
,workgroupCountY
,workgroupCountZ
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_dispatchWorkgroupsIndirect(
       JNIEnv *env , jobject obj,
    jobject
     indirectBuffer,
    jlong
     indirectOffset) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeIndirectBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(indirectBuffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuComputePassEncoderDispatchWorkgroupsIndirect(handle
, nativeIndirectBuffer
,indirectOffset
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_end(
       JNIEnv *env , jobject obj) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));


    wgpuComputePassEncoderEnd(handle
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_insertDebugMarker(
       JNIEnv *env , jobject obj,
    jstring
     markerLabel) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));

    const char *nativeMarkerLabel = getString(env, markerLabel);

    wgpuComputePassEncoderInsertDebugMarker(handle
, nativeMarkerLabel
);

    env->ReleaseStringUTFChars(markerLabel, nativeMarkerLabel);
}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_popDebugGroup(
       JNIEnv *env , jobject obj) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));


    wgpuComputePassEncoderPopDebugGroup(handle
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_pushDebugGroup(
       JNIEnv *env , jobject obj,
    jstring
     groupLabel) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));

    const char *nativeGroupLabel = getString(env, groupLabel);

    wgpuComputePassEncoderPushDebugGroup(handle
, nativeGroupLabel
);

    env->ReleaseStringUTFChars(groupLabel, nativeGroupLabel);
}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_setBindGroup(
       JNIEnv *env , jobject obj,
    jint
     groupIndex,
    jobject
     group,
    jlong
     dynamicOffsetCount,
    jintArray
     dynamicOffsets) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));
    jclass bindGroupClass = env->FindClass("android/dawn/BindGroup");
    jmethodID bindGroupGetHandle = env->GetMethodID(bindGroupClass, "getHandle", "()J");

    const WGPUBindGroup nativeGroup = reinterpret_cast<WGPUBindGroup>(env->CallLongMethod(group, env->GetMethodID(bindGroupClass, "getHandle", "()J")));    const uint32_t *nativeDynamicOffsets = dynamicOffsets ? reinterpret_cast<const uint32_t *>(env->GetIntArrayElements(dynamicOffsets, 0)) : nullptr;

    wgpuComputePassEncoderSetBindGroup(handle
, groupIndex
,nativeGroup
,dynamicOffsetCount
,nativeDynamicOffsets
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuComputePassEncoderSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_setPipeline(
       JNIEnv *env , jobject obj,
    jobject
     pipeline) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));
    jclass computePipelineClass = env->FindClass("android/dawn/ComputePipeline");
    jmethodID computePipelineGetHandle = env->GetMethodID(computePipelineClass, "getHandle", "()J");

    const WGPUComputePipeline nativePipeline = reinterpret_cast<WGPUComputePipeline>(env->CallLongMethod(pipeline, env->GetMethodID(computePipelineClass, "getHandle", "()J")));
    wgpuComputePassEncoderSetPipeline(handle
, nativePipeline
);

}

DEFAULT extern "C" void Java_android_dawn_ComputePassEncoder_writeTimestamp(
       JNIEnv *env , jobject obj,
    jobject
     querySet,
    jint
     queryIndex) {

    jclass computePassEncoderClass = env->FindClass("android/dawn/ComputePassEncoder");
    const WGPUComputePassEncoder handle = reinterpret_cast<WGPUComputePassEncoder>(env->CallLongMethod(obj, env->GetMethodID(computePassEncoderClass, "getHandle", "()J")));
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");

    const WGPUQuerySet nativeQuerySet = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, env->GetMethodID(querySetClass, "getHandle", "()J")));
    wgpuComputePassEncoderWriteTimestamp(handle
, nativeQuerySet
,queryIndex
);

}

DEFAULT extern "C" jobject Java_android_dawn_ComputePipeline_getBindGroupLayout(
       JNIEnv *env , jobject obj,
    jint
     groupIndex) {

    jclass computePipelineClass = env->FindClass("android/dawn/ComputePipeline");
    const WGPUComputePipeline handle = reinterpret_cast<WGPUComputePipeline>(env->CallLongMethod(obj, env->GetMethodID(computePipelineClass, "getHandle", "()J")));


    auto result = wgpuComputePipelineGetBindGroupLayout(handle
, groupIndex
);

    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    return env->NewObject(bindGroupLayoutClass, env->GetMethodID(bindGroupLayoutClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_ComputePipeline_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass computePipelineClass = env->FindClass("android/dawn/ComputePipeline");
    const WGPUComputePipeline handle = reinterpret_cast<WGPUComputePipeline>(env->CallLongMethod(obj, env->GetMethodID(computePipelineClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuComputePipelineSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jobject Java_android_dawn_Device_createBindGroup(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUBindGroupDescriptor* nativeDescriptor = descriptor ? new WGPUBindGroupDescriptor(convertBindGroupDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateBindGroup(handle
, nativeDescriptor
);

    jclass bindGroupClass = env->FindClass("android/dawn/BindGroup");
    return env->NewObject(bindGroupClass, env->GetMethodID(bindGroupClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createBindGroupLayout(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUBindGroupLayoutDescriptor* nativeDescriptor = descriptor ? new WGPUBindGroupLayoutDescriptor(convertBindGroupLayoutDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateBindGroupLayout(handle
, nativeDescriptor
);

    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    return env->NewObject(bindGroupLayoutClass, env->GetMethodID(bindGroupLayoutClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createBuffer(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUBufferDescriptor* nativeDescriptor = descriptor ? new WGPUBufferDescriptor(convertBufferDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateBuffer(handle
, nativeDescriptor
);

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    return env->NewObject(bufferClass, env->GetMethodID(bufferClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createCommandEncoder(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUCommandEncoderDescriptor* nativeDescriptor = descriptor ? new WGPUCommandEncoderDescriptor(convertCommandEncoderDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateCommandEncoder(handle
, nativeDescriptor
);

    jclass commandEncoderClass = env->FindClass("android/dawn/CommandEncoder");
    return env->NewObject(commandEncoderClass, env->GetMethodID(commandEncoderClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createComputePipeline(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUComputePipelineDescriptor* nativeDescriptor = descriptor ? new WGPUComputePipelineDescriptor(convertComputePipelineDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateComputePipeline(handle
, nativeDescriptor
);

    jclass computePipelineClass = env->FindClass("android/dawn/ComputePipeline");
    return env->NewObject(computePipelineClass, env->GetMethodID(computePipelineClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_Device_createComputePipelineAsync(
       JNIEnv *env , jobject obj,
    jobject
     descriptor,
    jobject
     callback) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUComputePipelineDescriptor* nativeDescriptor = descriptor ? new WGPUComputePipelineDescriptor(convertComputePipelineDescriptor(env, descriptor)) : nullptr;
    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUCreateComputePipelineAsyncCallback nativeCallback = [](
    WGPUCreatePipelineAsyncStatus status,
    WGPUComputePipeline pipeline,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass createComputePipelineAsyncCallbackClass = env->FindClass("android/dawn/CreateComputePipelineAsyncCallback");
    jclass computePipelineClass = env->FindClass("android/dawn/ComputePipeline");
    jmethodID id = env->GetMethodID(createComputePipelineAsyncCallbackClass, "callback", "(ILandroid/dawn/ComputePipeline;Ljava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
,
    env->NewObject(computePipelineClass, env->GetMethodID(computePipelineClass, "<init>", "(J)V"), reinterpret_cast<jlong>(pipeline))
,
    env->NewStringUTF(message)
);
    };

    wgpuDeviceCreateComputePipelineAsync(handle
, nativeDescriptor
,nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" jobject Java_android_dawn_Device_createErrorBuffer(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUBufferDescriptor* nativeDescriptor = descriptor ? new WGPUBufferDescriptor(convertBufferDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateErrorBuffer(handle
, nativeDescriptor
);

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    return env->NewObject(bufferClass, env->GetMethodID(bufferClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createErrorExternalTexture(
       JNIEnv *env , jobject obj) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    auto result = wgpuDeviceCreateErrorExternalTexture(handle
);

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    return env->NewObject(externalTextureClass, env->GetMethodID(externalTextureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createErrorShaderModule(
       JNIEnv *env , jobject obj,
    jobject
     descriptor,
    jstring
     errorMessage) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUShaderModuleDescriptor* nativeDescriptor = descriptor ? new WGPUShaderModuleDescriptor(convertShaderModuleDescriptor(env, descriptor)) : nullptr;
    const char *nativeErrorMessage = getString(env, errorMessage);

    auto result = wgpuDeviceCreateErrorShaderModule(handle
, nativeDescriptor
,nativeErrorMessage
);

    env->ReleaseStringUTFChars(errorMessage, nativeErrorMessage);
    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    return env->NewObject(shaderModuleClass, env->GetMethodID(shaderModuleClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createErrorTexture(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUTextureDescriptor* nativeDescriptor = descriptor ? new WGPUTextureDescriptor(convertTextureDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateErrorTexture(handle
, nativeDescriptor
);

    jclass textureClass = env->FindClass("android/dawn/Texture");
    return env->NewObject(textureClass, env->GetMethodID(textureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createExternalTexture(
       JNIEnv *env , jobject obj,
    jobject
     externalTextureDescriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUExternalTextureDescriptor* nativeExternalTextureDescriptor = externalTextureDescriptor ? new WGPUExternalTextureDescriptor(convertExternalTextureDescriptor(env, externalTextureDescriptor)) : nullptr;

    auto result = wgpuDeviceCreateExternalTexture(handle
, nativeExternalTextureDescriptor
);

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    return env->NewObject(externalTextureClass, env->GetMethodID(externalTextureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createPipelineLayout(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUPipelineLayoutDescriptor* nativeDescriptor = descriptor ? new WGPUPipelineLayoutDescriptor(convertPipelineLayoutDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreatePipelineLayout(handle
, nativeDescriptor
);

    jclass pipelineLayoutClass = env->FindClass("android/dawn/PipelineLayout");
    return env->NewObject(pipelineLayoutClass, env->GetMethodID(pipelineLayoutClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createQuerySet(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUQuerySetDescriptor* nativeDescriptor = descriptor ? new WGPUQuerySetDescriptor(convertQuerySetDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateQuerySet(handle
, nativeDescriptor
);

    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    return env->NewObject(querySetClass, env->GetMethodID(querySetClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createRenderBundleEncoder(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPURenderBundleEncoderDescriptor* nativeDescriptor = descriptor ? new WGPURenderBundleEncoderDescriptor(convertRenderBundleEncoderDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateRenderBundleEncoder(handle
, nativeDescriptor
);

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    return env->NewObject(renderBundleEncoderClass, env->GetMethodID(renderBundleEncoderClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createRenderPipeline(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPURenderPipelineDescriptor* nativeDescriptor = descriptor ? new WGPURenderPipelineDescriptor(convertRenderPipelineDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateRenderPipeline(handle
, nativeDescriptor
);

    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    return env->NewObject(renderPipelineClass, env->GetMethodID(renderPipelineClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_Device_createRenderPipelineAsync(
       JNIEnv *env , jobject obj,
    jobject
     descriptor,
    jobject
     callback) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPURenderPipelineDescriptor* nativeDescriptor = descriptor ? new WGPURenderPipelineDescriptor(convertRenderPipelineDescriptor(env, descriptor)) : nullptr;
    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUCreateRenderPipelineAsyncCallback nativeCallback = [](
    WGPUCreatePipelineAsyncStatus status,
    WGPURenderPipeline pipeline,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass createRenderPipelineAsyncCallbackClass = env->FindClass("android/dawn/CreateRenderPipelineAsyncCallback");
    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    jmethodID id = env->GetMethodID(createRenderPipelineAsyncCallbackClass, "callback", "(ILandroid/dawn/RenderPipeline;Ljava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
,
    env->NewObject(renderPipelineClass, env->GetMethodID(renderPipelineClass, "<init>", "(J)V"), reinterpret_cast<jlong>(pipeline))
,
    env->NewStringUTF(message)
);
    };

    wgpuDeviceCreateRenderPipelineAsync(handle
, nativeDescriptor
,nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" jobject Java_android_dawn_Device_createSampler(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUSamplerDescriptor* nativeDescriptor = descriptor ? new WGPUSamplerDescriptor(convertSamplerDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateSampler(handle
, nativeDescriptor
);

    jclass samplerClass = env->FindClass("android/dawn/Sampler");
    return env->NewObject(samplerClass, env->GetMethodID(samplerClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createShaderModule(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUShaderModuleDescriptor* nativeDescriptor = descriptor ? new WGPUShaderModuleDescriptor(convertShaderModuleDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateShaderModule(handle
, nativeDescriptor
);

    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    return env->NewObject(shaderModuleClass, env->GetMethodID(shaderModuleClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createSwapChain(
       JNIEnv *env , jobject obj,
    jobject
     surface,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));
    jclass surfaceClass = env->FindClass("android/dawn/Surface");
    jmethodID surfaceGetHandle = env->GetMethodID(surfaceClass, "getHandle", "()J");

    const WGPUSurface nativeSurface = reinterpret_cast<WGPUSurface>(env->CallLongMethod(surface, env->GetMethodID(surfaceClass, "getHandle", "()J")));    const WGPUSwapChainDescriptor* nativeDescriptor = descriptor ? new WGPUSwapChainDescriptor(convertSwapChainDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateSwapChain(handle
, nativeSurface
,nativeDescriptor
);

    jclass swapChainClass = env->FindClass("android/dawn/SwapChain");
    return env->NewObject(swapChainClass, env->GetMethodID(swapChainClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_createTexture(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUTextureDescriptor* nativeDescriptor = descriptor ? new WGPUTextureDescriptor(convertTextureDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceCreateTexture(handle
, nativeDescriptor
);

    jclass textureClass = env->FindClass("android/dawn/Texture");
    return env->NewObject(textureClass, env->GetMethodID(textureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_Device_destroy(
       JNIEnv *env , jobject obj) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    wgpuDeviceDestroy(handle
);

}

DEFAULT extern "C" void Java_android_dawn_Device_forceLoss(
       JNIEnv *env , jobject obj,
    jint
     type,
    jstring
     message) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const char *nativeMessage = getString(env, message);

    wgpuDeviceForceLoss(handle
, (WGPUDeviceLostReason) type
,nativeMessage
);

    env->ReleaseStringUTFChars(message, nativeMessage);
}

DEFAULT extern "C" jobject Java_android_dawn_Device_getAdapter(
       JNIEnv *env , jobject obj) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    auto result = wgpuDeviceGetAdapter(handle
);

    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    return env->NewObject(adapterClass, env->GetMethodID(adapterClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_getQueue(
       JNIEnv *env , jobject obj) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    auto result = wgpuDeviceGetQueue(handle
);

    jclass queueClass = env->FindClass("android/dawn/Queue");
    return env->NewObject(queueClass, env->GetMethodID(queueClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jint Java_android_dawn_Device_getSupportedSurfaceUsage(
       JNIEnv *env , jobject obj,
    jobject
     surface) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));
    jclass surfaceClass = env->FindClass("android/dawn/Surface");
    jmethodID surfaceGetHandle = env->GetMethodID(surfaceClass, "getHandle", "()J");

    const WGPUSurface nativeSurface = reinterpret_cast<WGPUSurface>(env->CallLongMethod(surface, env->GetMethodID(surfaceClass, "getHandle", "()J")));
    auto result = wgpuDeviceGetSupportedSurfaceUsage(handle
, nativeSurface
);

    return result;
}

DEFAULT extern "C" jboolean Java_android_dawn_Device_hasFeature(
       JNIEnv *env , jobject obj,
    jint
     feature) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    auto result = wgpuDeviceHasFeature(handle
, (WGPUFeatureName) feature
);

    return result;
}

DEFAULT extern "C" jobject Java_android_dawn_Device_importSharedBufferMemory(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUSharedBufferMemoryDescriptor* nativeDescriptor = descriptor ? new WGPUSharedBufferMemoryDescriptor(convertSharedBufferMemoryDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceImportSharedBufferMemory(handle
, nativeDescriptor
);

    jclass sharedBufferMemoryClass = env->FindClass("android/dawn/SharedBufferMemory");
    return env->NewObject(sharedBufferMemoryClass, env->GetMethodID(sharedBufferMemoryClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_importSharedFence(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUSharedFenceDescriptor* nativeDescriptor = descriptor ? new WGPUSharedFenceDescriptor(convertSharedFenceDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceImportSharedFence(handle
, nativeDescriptor
);

    jclass sharedFenceClass = env->FindClass("android/dawn/SharedFence");
    return env->NewObject(sharedFenceClass, env->GetMethodID(sharedFenceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Device_importSharedTextureMemory(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUSharedTextureMemoryDescriptor* nativeDescriptor = descriptor ? new WGPUSharedTextureMemoryDescriptor(convertSharedTextureMemoryDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuDeviceImportSharedTextureMemory(handle
, nativeDescriptor
);

    jclass sharedTextureMemoryClass = env->FindClass("android/dawn/SharedTextureMemory");
    return env->NewObject(sharedTextureMemoryClass, env->GetMethodID(sharedTextureMemoryClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_Device_injectError(
       JNIEnv *env , jobject obj,
    jint
     type,
    jstring
     message) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const char *nativeMessage = getString(env, message);

    wgpuDeviceInjectError(handle
, (WGPUErrorType) type
,nativeMessage
);

    env->ReleaseStringUTFChars(message, nativeMessage);
}

DEFAULT extern "C" void Java_android_dawn_Device_popErrorScope(
       JNIEnv *env , jobject obj,
    jobject
     oldCallback) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env};

    WGPUErrorCallback nativeOldCallback = [](
    WGPUErrorType type,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass errorCallbackClass = env->FindClass("android/dawn/ErrorCallback");
    jmethodID id = env->GetMethodID(errorCallbackClass, "callback", "(ILjava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(type)
,
    env->NewStringUTF(message)
);
    };

    wgpuDevicePopErrorScope(handle
, nativeOldCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_Device_pushErrorScope(
       JNIEnv *env , jobject obj,
    jint
     filter) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    wgpuDevicePushErrorScope(handle
, (WGPUErrorFilter) filter
);

}

DEFAULT extern "C" void Java_android_dawn_Device_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuDeviceSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Device_setLoggingCallback(
       JNIEnv *env , jobject obj,
    jobject
     callback) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPULoggingCallback nativeCallback = [](
    WGPULoggingType type,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass loggingCallbackClass = env->FindClass("android/dawn/LoggingCallback");
    jmethodID id = env->GetMethodID(loggingCallbackClass, "callback", "(ILjava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(type)
,
    env->NewStringUTF(message)
);
    };

    wgpuDeviceSetLoggingCallback(handle
, nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_Device_setUncapturedErrorCallback(
       JNIEnv *env , jobject obj,
    jobject
     callback) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUErrorCallback nativeCallback = [](
    WGPUErrorType type,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass errorCallbackClass = env->FindClass("android/dawn/ErrorCallback");
    jmethodID id = env->GetMethodID(errorCallbackClass, "callback", "(ILjava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(type)
,
    env->NewStringUTF(message)
);
    };

    wgpuDeviceSetUncapturedErrorCallback(handle
, nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_Device_tick(
       JNIEnv *env , jobject obj) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));


    wgpuDeviceTick(handle
);

}

DEFAULT extern "C" void Java_android_dawn_Device_validateTextureDescriptor(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass deviceClass = env->FindClass("android/dawn/Device");
    const WGPUDevice handle = reinterpret_cast<WGPUDevice>(env->CallLongMethod(obj, env->GetMethodID(deviceClass, "getHandle", "()J")));

    const WGPUTextureDescriptor* nativeDescriptor = descriptor ? new WGPUTextureDescriptor(convertTextureDescriptor(env, descriptor)) : nullptr;

    wgpuDeviceValidateTextureDescriptor(handle
, nativeDescriptor
);

}

DEFAULT extern "C" void Java_android_dawn_ExternalTexture_destroy(
       JNIEnv *env , jobject obj) {

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    const WGPUExternalTexture handle = reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(obj, env->GetMethodID(externalTextureClass, "getHandle", "()J")));


    wgpuExternalTextureDestroy(handle
);

}

DEFAULT extern "C" void Java_android_dawn_ExternalTexture_expire(
       JNIEnv *env , jobject obj) {

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    const WGPUExternalTexture handle = reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(obj, env->GetMethodID(externalTextureClass, "getHandle", "()J")));


    wgpuExternalTextureExpire(handle
);

}

DEFAULT extern "C" void Java_android_dawn_ExternalTexture_refresh(
       JNIEnv *env , jobject obj) {

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    const WGPUExternalTexture handle = reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(obj, env->GetMethodID(externalTextureClass, "getHandle", "()J")));


    wgpuExternalTextureRefresh(handle
);

}

DEFAULT extern "C" void Java_android_dawn_ExternalTexture_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass externalTextureClass = env->FindClass("android/dawn/ExternalTexture");
    const WGPUExternalTexture handle = reinterpret_cast<WGPUExternalTexture>(env->CallLongMethod(obj, env->GetMethodID(externalTextureClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuExternalTextureSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jobject Java_android_dawn_Instance_createSurface(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    const WGPUInstance handle = reinterpret_cast<WGPUInstance>(env->CallLongMethod(obj, env->GetMethodID(instanceClass, "getHandle", "()J")));

    const WGPUSurfaceDescriptor* nativeDescriptor = descriptor ? new WGPUSurfaceDescriptor(convertSurfaceDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuInstanceCreateSurface(handle
, nativeDescriptor
);

    jclass surfaceClass = env->FindClass("android/dawn/Surface");
    return env->NewObject(surfaceClass, env->GetMethodID(surfaceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jboolean Java_android_dawn_Instance_hasWGSLLanguageFeature(
       JNIEnv *env , jobject obj,
    jint
     feature) {

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    const WGPUInstance handle = reinterpret_cast<WGPUInstance>(env->CallLongMethod(obj, env->GetMethodID(instanceClass, "getHandle", "()J")));


    auto result = wgpuInstanceHasWGSLLanguageFeature(handle
, (WGPUWGSLFeatureName) feature
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_Instance_processEvents(
       JNIEnv *env , jobject obj) {

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    const WGPUInstance handle = reinterpret_cast<WGPUInstance>(env->CallLongMethod(obj, env->GetMethodID(instanceClass, "getHandle", "()J")));


    wgpuInstanceProcessEvents(handle
);

}

DEFAULT extern "C" void Java_android_dawn_Instance_requestAdapter(
       JNIEnv *env , jobject obj,
    jobject
     options,
    jobject
     callback) {

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    const WGPUInstance handle = reinterpret_cast<WGPUInstance>(env->CallLongMethod(obj, env->GetMethodID(instanceClass, "getHandle", "()J")));

    const WGPURequestAdapterOptions* nativeOptions = options ? new WGPURequestAdapterOptions(convertRequestAdapterOptions(env, options)) : nullptr;
    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPURequestAdapterCallback nativeCallback = [](
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    char const * message,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass requestAdapterCallbackClass = env->FindClass("android/dawn/RequestAdapterCallback");
    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    jmethodID id = env->GetMethodID(requestAdapterCallbackClass, "callback", "(ILandroid/dawn/Adapter;Ljava/lang/String;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
,
    env->NewObject(adapterClass, env->GetMethodID(adapterClass, "<init>", "(J)V"), reinterpret_cast<jlong>(adapter))
,
    env->NewStringUTF(message)
);
    };

    wgpuInstanceRequestAdapter(handle
, nativeOptions
,nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_PipelineLayout_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass pipelineLayoutClass = env->FindClass("android/dawn/PipelineLayout");
    const WGPUPipelineLayout handle = reinterpret_cast<WGPUPipelineLayout>(env->CallLongMethod(obj, env->GetMethodID(pipelineLayoutClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuPipelineLayoutSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_QuerySet_destroy(
       JNIEnv *env , jobject obj) {

    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    const WGPUQuerySet handle = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(obj, env->GetMethodID(querySetClass, "getHandle", "()J")));


    wgpuQuerySetDestroy(handle
);

}

DEFAULT extern "C" jint Java_android_dawn_QuerySet_getCount(
       JNIEnv *env , jobject obj) {

    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    const WGPUQuerySet handle = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(obj, env->GetMethodID(querySetClass, "getHandle", "()J")));


    auto result = wgpuQuerySetGetCount(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_QuerySet_getType(
       JNIEnv *env , jobject obj) {

    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    const WGPUQuerySet handle = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(obj, env->GetMethodID(querySetClass, "getHandle", "()J")));


    auto result = wgpuQuerySetGetType(handle
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_QuerySet_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    const WGPUQuerySet handle = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(obj, env->GetMethodID(querySetClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuQuerySetSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Queue_copyExternalTextureForBrowser(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jobject
     destination,
    jobject
     copySize,
    jobject
     options) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));

    const WGPUImageCopyExternalTexture* nativeSource = source ? new WGPUImageCopyExternalTexture(convertImageCopyExternalTexture(env, source)) : nullptr;
    const WGPUImageCopyTexture* nativeDestination = destination ? new WGPUImageCopyTexture(convertImageCopyTexture(env, destination)) : nullptr;
    const WGPUExtent3D* nativeCopySize = copySize ? new WGPUExtent3D(convertExtent3D(env, copySize)) : nullptr;
    const WGPUCopyTextureForBrowserOptions* nativeOptions = options ? new WGPUCopyTextureForBrowserOptions(convertCopyTextureForBrowserOptions(env, options)) : nullptr;

    wgpuQueueCopyExternalTextureForBrowser(handle
, nativeSource
,nativeDestination
,nativeCopySize
,nativeOptions
);

}

DEFAULT extern "C" void Java_android_dawn_Queue_copyTextureForBrowser(
       JNIEnv *env , jobject obj,
    jobject
     source,
    jobject
     destination,
    jobject
     copySize,
    jobject
     options) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));

    const WGPUImageCopyTexture* nativeSource = source ? new WGPUImageCopyTexture(convertImageCopyTexture(env, source)) : nullptr;
    const WGPUImageCopyTexture* nativeDestination = destination ? new WGPUImageCopyTexture(convertImageCopyTexture(env, destination)) : nullptr;
    const WGPUExtent3D* nativeCopySize = copySize ? new WGPUExtent3D(convertExtent3D(env, copySize)) : nullptr;
    const WGPUCopyTextureForBrowserOptions* nativeOptions = options ? new WGPUCopyTextureForBrowserOptions(convertCopyTextureForBrowserOptions(env, options)) : nullptr;

    wgpuQueueCopyTextureForBrowser(handle
, nativeSource
,nativeDestination
,nativeCopySize
,nativeOptions
);

}

DEFAULT extern "C" void Java_android_dawn_Queue_onSubmittedWorkDone(
       JNIEnv *env , jobject obj,
    jobject
     callback) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUQueueWorkDoneCallback nativeCallback = [](
    WGPUQueueWorkDoneStatus status,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass queueWorkDoneCallbackClass = env->FindClass("android/dawn/QueueWorkDoneCallback");
    jmethodID id = env->GetMethodID(queueWorkDoneCallbackClass, "callback", "(I)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
);
    };

    wgpuQueueOnSubmittedWorkDone(handle
, nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_Queue_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuQueueSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Queue_submit(
       JNIEnv *env , jobject obj,
    jlong
     commandCount,
    jobjectArray
     commands) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));
    jclass commandBufferClass = env->FindClass("android/dawn/CommandBuffer");
    jmethodID commandBufferGetHandle = env->GetMethodID(commandBufferClass, "getHandle", "()J");

    size_t commandsCount = env->GetArrayLength(commands);
    WGPUCommandBuffer* nativeCommands = new WGPUCommandBuffer[commandsCount];
    for (int idx = 0; idx != commandsCount; idx++) {
        nativeCommands[idx] = reinterpret_cast<WGPUCommandBuffer>(env->CallLongMethod(
                env->GetObjectArrayElement(commands, idx), commandBufferGetHandle));
    }

    wgpuQueueSubmit(handle
, commandCount
,reinterpret_cast<WGPUCommandBuffer*>(nativeCommands)
);

}

DEFAULT extern "C" void Java_android_dawn_Queue_writeBuffer(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jlong
     bufferOffset,
    jbyteArray
     data,
    jlong
     size) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));    const void *nativeData = env->GetByteArrayElements(data, 0);

    wgpuQueueWriteBuffer(handle
, nativeBuffer
,bufferOffset
,nativeData
,size
);

    env->ReleaseByteArrayElements(data, (jbyte*) nativeData, 0);
}

DEFAULT extern "C" void Java_android_dawn_Queue_writeTexture(
       JNIEnv *env , jobject obj,
    jobject
     destination,
    jbyteArray
     data,
    jlong
     dataSize,
    jobject
     dataLayout,
    jobject
     writeSize) {

    jclass queueClass = env->FindClass("android/dawn/Queue");
    const WGPUQueue handle = reinterpret_cast<WGPUQueue>(env->CallLongMethod(obj, env->GetMethodID(queueClass, "getHandle", "()J")));

    const WGPUImageCopyTexture* nativeDestination = destination ? new WGPUImageCopyTexture(convertImageCopyTexture(env, destination)) : nullptr;
    const void *nativeData = env->GetByteArrayElements(data, 0);
    const WGPUTextureDataLayout* nativeDataLayout = dataLayout ? new WGPUTextureDataLayout(convertTextureDataLayout(env, dataLayout)) : nullptr;
    const WGPUExtent3D* nativeWriteSize = writeSize ? new WGPUExtent3D(convertExtent3D(env, writeSize)) : nullptr;

    wgpuQueueWriteTexture(handle
, nativeDestination
,nativeData
,dataSize
,nativeDataLayout
,nativeWriteSize
);

    env->ReleaseByteArrayElements(data, (jbyte*) nativeData, 0);
}

DEFAULT extern "C" void Java_android_dawn_RenderBundle_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass renderBundleClass = env->FindClass("android/dawn/RenderBundle");
    const WGPURenderBundle handle = reinterpret_cast<WGPURenderBundle>(env->CallLongMethod(obj, env->GetMethodID(renderBundleClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuRenderBundleSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_draw(
       JNIEnv *env , jobject obj,
    jint
     vertexCount,
    jint
     instanceCount,
    jint
     firstVertex,
    jint
     firstInstance) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));


    wgpuRenderBundleEncoderDraw(handle
, vertexCount
,instanceCount
,firstVertex
,firstInstance
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_drawIndexed(
       JNIEnv *env , jobject obj,
    jint
     indexCount,
    jint
     instanceCount,
    jint
     firstIndex,
    jint
     baseVertex,
    jint
     firstInstance) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));


    wgpuRenderBundleEncoderDrawIndexed(handle
, indexCount
,instanceCount
,firstIndex
,baseVertex
,firstInstance
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_drawIndexedIndirect(
       JNIEnv *env , jobject obj,
    jobject
     indirectBuffer,
    jlong
     indirectOffset) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeIndirectBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(indirectBuffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderBundleEncoderDrawIndexedIndirect(handle
, nativeIndirectBuffer
,indirectOffset
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_drawIndirect(
       JNIEnv *env , jobject obj,
    jobject
     indirectBuffer,
    jlong
     indirectOffset) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeIndirectBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(indirectBuffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderBundleEncoderDrawIndirect(handle
, nativeIndirectBuffer
,indirectOffset
);

}

DEFAULT extern "C" jobject Java_android_dawn_RenderBundleEncoder_finish(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));

    const WGPURenderBundleDescriptor* nativeDescriptor = descriptor ? new WGPURenderBundleDescriptor(convertRenderBundleDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuRenderBundleEncoderFinish(handle
, nativeDescriptor
);

    jclass renderBundleClass = env->FindClass("android/dawn/RenderBundle");
    return env->NewObject(renderBundleClass, env->GetMethodID(renderBundleClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_insertDebugMarker(
       JNIEnv *env , jobject obj,
    jstring
     markerLabel) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));

    const char *nativeMarkerLabel = getString(env, markerLabel);

    wgpuRenderBundleEncoderInsertDebugMarker(handle
, nativeMarkerLabel
);

    env->ReleaseStringUTFChars(markerLabel, nativeMarkerLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_popDebugGroup(
       JNIEnv *env , jobject obj) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));


    wgpuRenderBundleEncoderPopDebugGroup(handle
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_pushDebugGroup(
       JNIEnv *env , jobject obj,
    jstring
     groupLabel) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));

    const char *nativeGroupLabel = getString(env, groupLabel);

    wgpuRenderBundleEncoderPushDebugGroup(handle
, nativeGroupLabel
);

    env->ReleaseStringUTFChars(groupLabel, nativeGroupLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_setBindGroup(
       JNIEnv *env , jobject obj,
    jint
     groupIndex,
    jobject
     group,
    jlong
     dynamicOffsetCount,
    jintArray
     dynamicOffsets) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass bindGroupClass = env->FindClass("android/dawn/BindGroup");
    jmethodID bindGroupGetHandle = env->GetMethodID(bindGroupClass, "getHandle", "()J");

    const WGPUBindGroup nativeGroup = reinterpret_cast<WGPUBindGroup>(env->CallLongMethod(group, env->GetMethodID(bindGroupClass, "getHandle", "()J")));    const uint32_t *nativeDynamicOffsets = dynamicOffsets ? reinterpret_cast<const uint32_t *>(env->GetIntArrayElements(dynamicOffsets, 0)) : nullptr;

    wgpuRenderBundleEncoderSetBindGroup(handle
, groupIndex
,nativeGroup
,dynamicOffsetCount
,nativeDynamicOffsets
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_setIndexBuffer(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jint
     format,
    jlong
     offset,
    jlong
     size) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderBundleEncoderSetIndexBuffer(handle
, nativeBuffer
,(WGPUIndexFormat) format
,offset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuRenderBundleEncoderSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_setPipeline(
       JNIEnv *env , jobject obj,
    jobject
     pipeline) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    jmethodID renderPipelineGetHandle = env->GetMethodID(renderPipelineClass, "getHandle", "()J");

    const WGPURenderPipeline nativePipeline = reinterpret_cast<WGPURenderPipeline>(env->CallLongMethod(pipeline, env->GetMethodID(renderPipelineClass, "getHandle", "()J")));
    wgpuRenderBundleEncoderSetPipeline(handle
, nativePipeline
);

}

DEFAULT extern "C" void Java_android_dawn_RenderBundleEncoder_setVertexBuffer(
       JNIEnv *env , jobject obj,
    jint
     slot,
    jobject
     buffer,
    jlong
     offset,
    jlong
     size) {

    jclass renderBundleEncoderClass = env->FindClass("android/dawn/RenderBundleEncoder");
    const WGPURenderBundleEncoder handle = reinterpret_cast<WGPURenderBundleEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderBundleEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderBundleEncoderSetVertexBuffer(handle
, slot
,nativeBuffer
,offset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_beginOcclusionQuery(
       JNIEnv *env , jobject obj,
    jint
     queryIndex) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderBeginOcclusionQuery(handle
, queryIndex
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_draw(
       JNIEnv *env , jobject obj,
    jint
     vertexCount,
    jint
     instanceCount,
    jint
     firstVertex,
    jint
     firstInstance) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderDraw(handle
, vertexCount
,instanceCount
,firstVertex
,firstInstance
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_drawIndexed(
       JNIEnv *env , jobject obj,
    jint
     indexCount,
    jint
     instanceCount,
    jint
     firstIndex,
    jint
     baseVertex,
    jint
     firstInstance) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderDrawIndexed(handle
, indexCount
,instanceCount
,firstIndex
,baseVertex
,firstInstance
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_drawIndexedIndirect(
       JNIEnv *env , jobject obj,
    jobject
     indirectBuffer,
    jlong
     indirectOffset) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeIndirectBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(indirectBuffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderPassEncoderDrawIndexedIndirect(handle
, nativeIndirectBuffer
,indirectOffset
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_drawIndirect(
       JNIEnv *env , jobject obj,
    jobject
     indirectBuffer,
    jlong
     indirectOffset) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeIndirectBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(indirectBuffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderPassEncoderDrawIndirect(handle
, nativeIndirectBuffer
,indirectOffset
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_end(
       JNIEnv *env , jobject obj) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderEnd(handle
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_endOcclusionQuery(
       JNIEnv *env , jobject obj) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderEndOcclusionQuery(handle
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_executeBundles(
       JNIEnv *env , jobject obj,
    jlong
     bundleCount,
    jobjectArray
     bundles) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass renderBundleClass = env->FindClass("android/dawn/RenderBundle");
    jmethodID renderBundleGetHandle = env->GetMethodID(renderBundleClass, "getHandle", "()J");

    size_t bundlesCount = env->GetArrayLength(bundles);
    WGPURenderBundle* nativeBundles = new WGPURenderBundle[bundlesCount];
    for (int idx = 0; idx != bundlesCount; idx++) {
        nativeBundles[idx] = reinterpret_cast<WGPURenderBundle>(env->CallLongMethod(
                env->GetObjectArrayElement(bundles, idx), renderBundleGetHandle));
    }

    wgpuRenderPassEncoderExecuteBundles(handle
, bundleCount
,reinterpret_cast<WGPURenderBundle*>(nativeBundles)
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_insertDebugMarker(
       JNIEnv *env , jobject obj,
    jstring
     markerLabel) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));

    const char *nativeMarkerLabel = getString(env, markerLabel);

    wgpuRenderPassEncoderInsertDebugMarker(handle
, nativeMarkerLabel
);

    env->ReleaseStringUTFChars(markerLabel, nativeMarkerLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_pixelLocalStorageBarrier(
       JNIEnv *env , jobject obj) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderPixelLocalStorageBarrier(handle
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_popDebugGroup(
       JNIEnv *env , jobject obj) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderPopDebugGroup(handle
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_pushDebugGroup(
       JNIEnv *env , jobject obj,
    jstring
     groupLabel) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));

    const char *nativeGroupLabel = getString(env, groupLabel);

    wgpuRenderPassEncoderPushDebugGroup(handle
, nativeGroupLabel
);

    env->ReleaseStringUTFChars(groupLabel, nativeGroupLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setBindGroup(
       JNIEnv *env , jobject obj,
    jint
     groupIndex,
    jobject
     group,
    jlong
     dynamicOffsetCount,
    jintArray
     dynamicOffsets) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass bindGroupClass = env->FindClass("android/dawn/BindGroup");
    jmethodID bindGroupGetHandle = env->GetMethodID(bindGroupClass, "getHandle", "()J");

    const WGPUBindGroup nativeGroup = reinterpret_cast<WGPUBindGroup>(env->CallLongMethod(group, env->GetMethodID(bindGroupClass, "getHandle", "()J")));    const uint32_t *nativeDynamicOffsets = dynamicOffsets ? reinterpret_cast<const uint32_t *>(env->GetIntArrayElements(dynamicOffsets, 0)) : nullptr;

    wgpuRenderPassEncoderSetBindGroup(handle
, groupIndex
,nativeGroup
,dynamicOffsetCount
,nativeDynamicOffsets
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setBlendConstant(
       JNIEnv *env , jobject obj,
    jobject
     color) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));

    const WGPUColor* nativeColor = color ? new WGPUColor(convertColor(env, color)) : nullptr;

    wgpuRenderPassEncoderSetBlendConstant(handle
, nativeColor
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setIndexBuffer(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jint
     format,
    jlong
     offset,
    jlong
     size) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderPassEncoderSetIndexBuffer(handle
, nativeBuffer
,(WGPUIndexFormat) format
,offset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuRenderPassEncoderSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setPipeline(
       JNIEnv *env , jobject obj,
    jobject
     pipeline) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    jmethodID renderPipelineGetHandle = env->GetMethodID(renderPipelineClass, "getHandle", "()J");

    const WGPURenderPipeline nativePipeline = reinterpret_cast<WGPURenderPipeline>(env->CallLongMethod(pipeline, env->GetMethodID(renderPipelineClass, "getHandle", "()J")));
    wgpuRenderPassEncoderSetPipeline(handle
, nativePipeline
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setScissorRect(
       JNIEnv *env , jobject obj,
    jint
     x,
    jint
     y,
    jint
     width,
    jint
     height) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderSetScissorRect(handle
, x
,y
,width
,height
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setStencilReference(
       JNIEnv *env , jobject obj,
    jint
     reference) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderSetStencilReference(handle
, reference
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setVertexBuffer(
       JNIEnv *env , jobject obj,
    jint
     slot,
    jobject
     buffer,
    jlong
     offset,
    jlong
     size) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));
    wgpuRenderPassEncoderSetVertexBuffer(handle
, slot
,nativeBuffer
,offset
,size
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_setViewport(
       JNIEnv *env , jobject obj,
    jfloat
     x,
    jfloat
     y,
    jfloat
     width,
    jfloat
     height,
    jfloat
     minDepth,
    jfloat
     maxDepth) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));


    wgpuRenderPassEncoderSetViewport(handle
, x
,y
,width
,height
,minDepth
,maxDepth
);

}

DEFAULT extern "C" void Java_android_dawn_RenderPassEncoder_writeTimestamp(
       JNIEnv *env , jobject obj,
    jobject
     querySet,
    jint
     queryIndex) {

    jclass renderPassEncoderClass = env->FindClass("android/dawn/RenderPassEncoder");
    const WGPURenderPassEncoder handle = reinterpret_cast<WGPURenderPassEncoder>(env->CallLongMethod(obj, env->GetMethodID(renderPassEncoderClass, "getHandle", "()J")));
    jclass querySetClass = env->FindClass("android/dawn/QuerySet");
    jmethodID querySetGetHandle = env->GetMethodID(querySetClass, "getHandle", "()J");

    const WGPUQuerySet nativeQuerySet = reinterpret_cast<WGPUQuerySet>(env->CallLongMethod(querySet, env->GetMethodID(querySetClass, "getHandle", "()J")));
    wgpuRenderPassEncoderWriteTimestamp(handle
, nativeQuerySet
,queryIndex
);

}

DEFAULT extern "C" jobject Java_android_dawn_RenderPipeline_getBindGroupLayout(
       JNIEnv *env , jobject obj,
    jint
     groupIndex) {

    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    const WGPURenderPipeline handle = reinterpret_cast<WGPURenderPipeline>(env->CallLongMethod(obj, env->GetMethodID(renderPipelineClass, "getHandle", "()J")));


    auto result = wgpuRenderPipelineGetBindGroupLayout(handle
, groupIndex
);

    jclass bindGroupLayoutClass = env->FindClass("android/dawn/BindGroupLayout");
    return env->NewObject(bindGroupLayoutClass, env->GetMethodID(bindGroupLayoutClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_RenderPipeline_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass renderPipelineClass = env->FindClass("android/dawn/RenderPipeline");
    const WGPURenderPipeline handle = reinterpret_cast<WGPURenderPipeline>(env->CallLongMethod(obj, env->GetMethodID(renderPipelineClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuRenderPipelineSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_Sampler_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass samplerClass = env->FindClass("android/dawn/Sampler");
    const WGPUSampler handle = reinterpret_cast<WGPUSampler>(env->CallLongMethod(obj, env->GetMethodID(samplerClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuSamplerSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_ShaderModule_getCompilationInfo(
       JNIEnv *env , jobject obj,
    jobject
     callback) {

    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    const WGPUShaderModule handle = reinterpret_cast<WGPUShaderModule>(env->CallLongMethod(obj, env->GetMethodID(shaderModuleClass, "getHandle", "()J")));

    struct UserData nativeUserdata = {.env = env, .callback = callback};

    WGPUCompilationInfoCallback nativeCallback = [](
    WGPUCompilationInfoRequestStatus status,
    WGPUCompilationInfo const * compilationInfo,
    void * userdata
) {
    UserData* userData1 = static_cast<UserData *>(userdata);
    JNIEnv *env = userData1->env;
    jclass compilationInfoCallbackClass = env->FindClass("android/dawn/CompilationInfoCallback");
    jmethodID id = env->GetMethodID(compilationInfoCallbackClass, "callback", "(ILandroid/dawn/CompilationInfo;)V");
    env->CallVoidMethod(userData1->callback, id
,
    jint(status)
,
    jlong(compilationInfo)  );
    };

    wgpuShaderModuleGetCompilationInfo(handle
, nativeCallback
,&nativeUserdata
);

}

DEFAULT extern "C" void Java_android_dawn_ShaderModule_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass shaderModuleClass = env->FindClass("android/dawn/ShaderModule");
    const WGPUShaderModule handle = reinterpret_cast<WGPUShaderModule>(env->CallLongMethod(obj, env->GetMethodID(shaderModuleClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuShaderModuleSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jboolean Java_android_dawn_SharedBufferMemory_beginAccess(
       JNIEnv *env , jobject obj,
    jobject
     buffer,
    jobject
     descriptor) {

    jclass sharedBufferMemoryClass = env->FindClass("android/dawn/SharedBufferMemory");
    const WGPUSharedBufferMemory handle = reinterpret_cast<WGPUSharedBufferMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedBufferMemoryClass, "getHandle", "()J")));
    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    jmethodID bufferGetHandle = env->GetMethodID(bufferClass, "getHandle", "()J");

    const WGPUBuffer nativeBuffer = reinterpret_cast<WGPUBuffer>(env->CallLongMethod(buffer, env->GetMethodID(bufferClass, "getHandle", "()J")));    const WGPUSharedBufferMemoryBeginAccessDescriptor* nativeDescriptor = descriptor ? new WGPUSharedBufferMemoryBeginAccessDescriptor(convertSharedBufferMemoryBeginAccessDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuSharedBufferMemoryBeginAccess(handle
, nativeBuffer
,nativeDescriptor
);

    return result;
}

DEFAULT extern "C" jobject Java_android_dawn_SharedBufferMemory_createBuffer(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass sharedBufferMemoryClass = env->FindClass("android/dawn/SharedBufferMemory");
    const WGPUSharedBufferMemory handle = reinterpret_cast<WGPUSharedBufferMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedBufferMemoryClass, "getHandle", "()J")));

    const WGPUBufferDescriptor* nativeDescriptor = descriptor ? new WGPUBufferDescriptor(convertBufferDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuSharedBufferMemoryCreateBuffer(handle
, nativeDescriptor
);

    jclass bufferClass = env->FindClass("android/dawn/Buffer");
    return env->NewObject(bufferClass, env->GetMethodID(bufferClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jboolean Java_android_dawn_SharedBufferMemory_isDeviceLost(
       JNIEnv *env , jobject obj) {

    jclass sharedBufferMemoryClass = env->FindClass("android/dawn/SharedBufferMemory");
    const WGPUSharedBufferMemory handle = reinterpret_cast<WGPUSharedBufferMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedBufferMemoryClass, "getHandle", "()J")));


    auto result = wgpuSharedBufferMemoryIsDeviceLost(handle
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_SharedBufferMemory_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass sharedBufferMemoryClass = env->FindClass("android/dawn/SharedBufferMemory");
    const WGPUSharedBufferMemory handle = reinterpret_cast<WGPUSharedBufferMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedBufferMemoryClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuSharedBufferMemorySetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jboolean Java_android_dawn_SharedTextureMemory_beginAccess(
       JNIEnv *env , jobject obj,
    jobject
     texture,
    jobject
     descriptor) {

    jclass sharedTextureMemoryClass = env->FindClass("android/dawn/SharedTextureMemory");
    const WGPUSharedTextureMemory handle = reinterpret_cast<WGPUSharedTextureMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryClass, "getHandle", "()J")));
    jclass textureClass = env->FindClass("android/dawn/Texture");
    jmethodID textureGetHandle = env->GetMethodID(textureClass, "getHandle", "()J");

    const WGPUTexture nativeTexture = reinterpret_cast<WGPUTexture>(env->CallLongMethod(texture, env->GetMethodID(textureClass, "getHandle", "()J")));    const WGPUSharedTextureMemoryBeginAccessDescriptor* nativeDescriptor = descriptor ? new WGPUSharedTextureMemoryBeginAccessDescriptor(convertSharedTextureMemoryBeginAccessDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuSharedTextureMemoryBeginAccess(handle
, nativeTexture
,nativeDescriptor
);

    return result;
}

DEFAULT extern "C" jobject Java_android_dawn_SharedTextureMemory_createTexture(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass sharedTextureMemoryClass = env->FindClass("android/dawn/SharedTextureMemory");
    const WGPUSharedTextureMemory handle = reinterpret_cast<WGPUSharedTextureMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryClass, "getHandle", "()J")));

    const WGPUTextureDescriptor* nativeDescriptor = descriptor ? new WGPUTextureDescriptor(convertTextureDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuSharedTextureMemoryCreateTexture(handle
, nativeDescriptor
);

    jclass textureClass = env->FindClass("android/dawn/Texture");
    return env->NewObject(textureClass, env->GetMethodID(textureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jboolean Java_android_dawn_SharedTextureMemory_isDeviceLost(
       JNIEnv *env , jobject obj) {

    jclass sharedTextureMemoryClass = env->FindClass("android/dawn/SharedTextureMemory");
    const WGPUSharedTextureMemory handle = reinterpret_cast<WGPUSharedTextureMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryClass, "getHandle", "()J")));


    auto result = wgpuSharedTextureMemoryIsDeviceLost(handle
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_SharedTextureMemory_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass sharedTextureMemoryClass = env->FindClass("android/dawn/SharedTextureMemory");
    const WGPUSharedTextureMemory handle = reinterpret_cast<WGPUSharedTextureMemory>(env->CallLongMethod(obj, env->GetMethodID(sharedTextureMemoryClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuSharedTextureMemorySetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" jint Java_android_dawn_Surface_getPreferredFormat(
       JNIEnv *env , jobject obj,
    jobject
     adapter) {

    jclass surfaceClass = env->FindClass("android/dawn/Surface");
    const WGPUSurface handle = reinterpret_cast<WGPUSurface>(env->CallLongMethod(obj, env->GetMethodID(surfaceClass, "getHandle", "()J")));
    jclass adapterClass = env->FindClass("android/dawn/Adapter");
    jmethodID adapterGetHandle = env->GetMethodID(adapterClass, "getHandle", "()J");

    const WGPUAdapter nativeAdapter = reinterpret_cast<WGPUAdapter>(env->CallLongMethod(adapter, env->GetMethodID(adapterClass, "getHandle", "()J")));
    auto result = wgpuSurfaceGetPreferredFormat(handle
, nativeAdapter
);

    return result;
}

DEFAULT extern "C" jobject Java_android_dawn_SwapChain_getCurrentTexture(
       JNIEnv *env , jobject obj) {

    jclass swapChainClass = env->FindClass("android/dawn/SwapChain");
    const WGPUSwapChain handle = reinterpret_cast<WGPUSwapChain>(env->CallLongMethod(obj, env->GetMethodID(swapChainClass, "getHandle", "()J")));


    auto result = wgpuSwapChainGetCurrentTexture(handle
);

    jclass textureClass = env->FindClass("android/dawn/Texture");
    return env->NewObject(textureClass, env->GetMethodID(textureClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_SwapChain_getCurrentTextureView(
       JNIEnv *env , jobject obj) {

    jclass swapChainClass = env->FindClass("android/dawn/SwapChain");
    const WGPUSwapChain handle = reinterpret_cast<WGPUSwapChain>(env->CallLongMethod(obj, env->GetMethodID(swapChainClass, "getHandle", "()J")));


    auto result = wgpuSwapChainGetCurrentTextureView(handle
);

    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    return env->NewObject(textureViewClass, env->GetMethodID(textureViewClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_SwapChain_present(
       JNIEnv *env , jobject obj) {

    jclass swapChainClass = env->FindClass("android/dawn/SwapChain");
    const WGPUSwapChain handle = reinterpret_cast<WGPUSwapChain>(env->CallLongMethod(obj, env->GetMethodID(swapChainClass, "getHandle", "()J")));


    wgpuSwapChainPresent(handle
);

}

DEFAULT extern "C" jobject Java_android_dawn_Texture_createErrorView(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));

    const WGPUTextureViewDescriptor* nativeDescriptor = descriptor ? new WGPUTextureViewDescriptor(convertTextureViewDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuTextureCreateErrorView(handle
, nativeDescriptor
);

    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    return env->NewObject(textureViewClass, env->GetMethodID(textureViewClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" jobject Java_android_dawn_Texture_createView(
       JNIEnv *env , jobject obj,
    jobject
     descriptor) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));

    const WGPUTextureViewDescriptor* nativeDescriptor = descriptor ? new WGPUTextureViewDescriptor(convertTextureViewDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuTextureCreateView(handle
, nativeDescriptor
);

    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    return env->NewObject(textureViewClass, env->GetMethodID(textureViewClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

DEFAULT extern "C" void Java_android_dawn_Texture_destroy(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    wgpuTextureDestroy(handle
);

}

DEFAULT extern "C" jint Java_android_dawn_Texture_getDepthOrArrayLayers(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetDepthOrArrayLayers(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getDimension(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetDimension(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getFormat(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetFormat(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getHeight(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetHeight(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getMipLevelCount(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetMipLevelCount(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getSampleCount(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetSampleCount(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getUsage(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetUsage(handle
);

    return result;
}

DEFAULT extern "C" jint Java_android_dawn_Texture_getWidth(
       JNIEnv *env , jobject obj) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));


    auto result = wgpuTextureGetWidth(handle
);

    return result;
}

DEFAULT extern "C" void Java_android_dawn_Texture_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass textureClass = env->FindClass("android/dawn/Texture");
    const WGPUTexture handle = reinterpret_cast<WGPUTexture>(env->CallLongMethod(obj, env->GetMethodID(textureClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuTextureSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}

DEFAULT extern "C" void Java_android_dawn_TextureView_setLabel(
       JNIEnv *env , jobject obj,
    jstring
     label) {

    jclass textureViewClass = env->FindClass("android/dawn/TextureView");
    const WGPUTextureView handle = reinterpret_cast<WGPUTextureView>(env->CallLongMethod(obj, env->GetMethodID(textureViewClass, "getHandle", "()J")));

    const char *nativeLabel = getString(env, label);

    wgpuTextureViewSetLabel(handle
, nativeLabel
);

    env->ReleaseStringUTFChars(label, nativeLabel);
}


DEFAULT extern "C" jobject Java_android_dawn_Functions_createInstance(
       JNIEnv *env , jclass clazz,
    jobject
     descriptor) {


    const WGPUInstanceDescriptor* nativeDescriptor = descriptor ? new WGPUInstanceDescriptor(convertInstanceDescriptor(env, descriptor)) : nullptr;

    auto result = wgpuCreateInstance(nativeDescriptor
);

    jclass instanceClass = env->FindClass("android/dawn/Instance");
    return env->NewObject(instanceClass, env->GetMethodID(instanceClass, "<init>", "(J)V"), reinterpret_cast<jlong>(result));}

