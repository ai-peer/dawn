{{{

const structInfo32 = (
{
    "defines": {},
    "structs": {
        "WGPUAdapterProperties": {
            "__size__": 40,
            "adapterType": 28,
            "architecture": 12,
            "backendType": 32,
            "compatibilityMode": 36,
            "deviceID": 16,
            "driverDescription": 24,
            "name": 20,
            "nextInChain": 0,
            "vendorID": 4,
            "vendorName": 8
        },
        "WGPUBindGroupDescriptor": {
            "__size__": 20,
            "entries": 16,
            "entryCount": 12,
            "label": 4,
            "layout": 8,
            "nextInChain": 0
        },
        "WGPUBindGroupEntry": {
            "__size__": 40,
            "binding": 4,
            "buffer": 8,
            "nextInChain": 0,
            "offset": 16,
            "sampler": 32,
            "size": 24,
            "textureView": 36
        },
        "WGPUBindGroupLayoutDescriptor": {
            "__size__": 16,
            "entries": 12,
            "entryCount": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUBindGroupLayoutEntry": {
            "__size__": 80,
            "binding": 4,
            "buffer": 16,
            "nextInChain": 0,
            "sampler": 40,
            "storageTexture": 64,
            "texture": 48,
            "visibility": 8
        },
        "WGPUBlendComponent": {
            "__size__": 12,
            "dstFactor": 8,
            "operation": 0,
            "srcFactor": 4
        },
        "WGPUBlendState": {
            "__size__": 24,
            "alpha": 12,
            "color": 0
        },
        "WGPUBufferBindingLayout": {
            "__size__": 24,
            "hasDynamicOffset": 8,
            "minBindingSize": 16,
            "nextInChain": 0,
            "type": 4
        },
        "WGPUBufferDescriptor": {
            "__size__": 32,
            "label": 4,
            "mappedAtCreation": 24,
            "nextInChain": 0,
            "size": 16,
            "usage": 8
        },
        "WGPUBufferMapCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPUChainedStruct": {
            "__size__": 8,
            "next": 0,
            "sType": 4
        },
        "WGPUColor": {
            "__size__": 32,
            "a": 24,
            "b": 16,
            "g": 8,
            "r": 0
        },
        "WGPUColorTargetState": {
            "__size__": 16,
            "blend": 8,
            "format": 4,
            "nextInChain": 0,
            "writeMask": 12
        },
        "WGPUCommandBufferDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUCommandEncoderDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUCompilationInfo": {
            "__size__": 12,
            "messageCount": 4,
            "messages": 8,
            "nextInChain": 0
        },
        "WGPUCompilationInfoCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPUCompilationMessage": {
            "__size__": 72,
            "length": 40,
            "lineNum": 16,
            "linePos": 24,
            "message": 4,
            "nextInChain": 0,
            "offset": 32,
            "type": 8,
            "utf16Length": 64,
            "utf16LinePos": 48,
            "utf16Offset": 56
        },
        "WGPUComputePassDescriptor": {
            "__size__": 12,
            "label": 4,
            "nextInChain": 0,
            "timestampWrites": 8
        },
        "WGPUComputePassTimestampWrites": {
            "__size__": 12,
            "beginningOfPassWriteIndex": 4,
            "endOfPassWriteIndex": 8,
            "querySet": 0
        },
        "WGPUComputePipelineDescriptor": {
            "__size__": 32,
            "compute": 12,
            "label": 4,
            "layout": 8,
            "nextInChain": 0
        },
        "WGPUConstantEntry": {
            "__size__": 16,
            "key": 4,
            "nextInChain": 0,
            "value": 8
        },
        "WGPUCreateComputePipelineAsyncCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPUCreateRenderPipelineAsyncCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPUDepthStencilState": {
            "__size__": 68,
            "depthBias": 56,
            "depthBiasClamp": 64,
            "depthBiasSlopeScale": 60,
            "depthCompare": 12,
            "depthWriteEnabled": 8,
            "format": 4,
            "nextInChain": 0,
            "stencilBack": 32,
            "stencilFront": 16,
            "stencilReadMask": 48,
            "stencilWriteMask": 52
        },
        "WGPUDeviceDescriptor": {
            "__size__": 56,
            "defaultQueue": 20,
            "deviceLostCallbackInfo": 28,
            "label": 4,
            "nextInChain": 0,
            "requiredFeatureCount": 8,
            "requiredFeatures": 12,
            "requiredLimits": 16,
            "uncapturedErrorCallbackInfo": 44
        },
        "WGPUDeviceLostCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPUExtent3D": {
            "__size__": 12,
            "depthOrArrayLayers": 8,
            "height": 4,
            "width": 0
        },
        "WGPUFragmentState": {
            "__size__": 28,
            "constantCount": 12,
            "constants": 16,
            "entryPoint": 8,
            "module": 4,
            "nextInChain": 0,
            "targetCount": 20,
            "targets": 24
        },
        "WGPUFuture": {
            "__size__": 8,
            "id": 0
        },
        "WGPUFutureWaitInfo": {
            "__size__": 16,
            "completed": 8,
            "future": 0
        },
        "WGPUINTERNAL__USING_DAWN_WEBGPU": {
            "__size__": 0
        },
        "WGPUImageCopyBuffer": {
            "__size__": 32,
            "buffer": 24,
            "layout": 0
        },
        "WGPUImageCopyTexture": {
            "__size__": 24,
            "aspect": 20,
            "mipLevel": 4,
            "origin": 8,
            "texture": 0
        },
        "WGPUInstanceDescriptor": {
            "__size__": 16,
            "features": 4,
            "nextInChain": 0
        },
        "WGPUInstanceFeatures": {
            "__size__": 12,
            "nextInChain": 0,
            "timedWaitAnyEnable": 4,
            "timedWaitAnyMaxCount": 8
        },
        "WGPULimits": {
            "__size__": 144,
            "maxBindGroups": 16,
            "maxBindGroupsPlusVertexBuffers": 20,
            "maxBindingsPerBindGroup": 24,
            "maxBufferSize": 88,
            "maxColorAttachmentBytesPerSample": 116,
            "maxColorAttachments": 112,
            "maxComputeInvocationsPerWorkgroup": 124,
            "maxComputeWorkgroupSizeX": 128,
            "maxComputeWorkgroupSizeY": 132,
            "maxComputeWorkgroupSizeZ": 136,
            "maxComputeWorkgroupStorageSize": 120,
            "maxComputeWorkgroupsPerDimension": 140,
            "maxDynamicStorageBuffersPerPipelineLayout": 32,
            "maxDynamicUniformBuffersPerPipelineLayout": 28,
            "maxInterStageShaderComponents": 104,
            "maxInterStageShaderVariables": 108,
            "maxSampledTexturesPerShaderStage": 36,
            "maxSamplersPerShaderStage": 40,
            "maxStorageBufferBindingSize": 64,
            "maxStorageBuffersPerShaderStage": 44,
            "maxStorageTexturesPerShaderStage": 48,
            "maxTextureArrayLayers": 12,
            "maxTextureDimension1D": 0,
            "maxTextureDimension2D": 4,
            "maxTextureDimension3D": 8,
            "maxUniformBufferBindingSize": 56,
            "maxUniformBuffersPerShaderStage": 52,
            "maxVertexAttributes": 96,
            "maxVertexBufferArrayStride": 100,
            "maxVertexBuffers": 80,
            "minStorageBufferOffsetAlignment": 76,
            "minUniformBufferOffsetAlignment": 72
        },
        "WGPUMultisampleState": {
            "__size__": 16,
            "alphaToCoverageEnabled": 12,
            "count": 4,
            "mask": 8,
            "nextInChain": 0
        },
        "WGPUOrigin3D": {
            "__size__": 12,
            "x": 0,
            "y": 4,
            "z": 8
        },
        "WGPUPipelineLayoutDescriptor": {
            "__size__": 16,
            "bindGroupLayoutCount": 8,
            "bindGroupLayouts": 12,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUPopErrorScopeCallbackInfo": {
            "__size__": 20,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "oldCallback": 12,
            "userdata": 16
        },
        "WGPUPrimitiveDepthClipControl": {
            "__size__": 12,
            "chain": 0,
            "unclippedDepth": 8
        },
        "WGPUPrimitiveState": {
            "__size__": 20,
            "cullMode": 16,
            "frontFace": 12,
            "nextInChain": 0,
            "stripIndexFormat": 8,
            "topology": 4
        },
        "WGPUProgrammableStageDescriptor": {
            "__size__": 20,
            "constantCount": 12,
            "constants": 16,
            "entryPoint": 8,
            "module": 4,
            "nextInChain": 0
        },
        "WGPUQuerySetDescriptor": {
            "__size__": 16,
            "count": 12,
            "label": 4,
            "nextInChain": 0,
            "type": 8
        },
        "WGPUQueueDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUQueueWorkDoneCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPURenderBundleDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPURenderBundleEncoderDescriptor": {
            "__size__": 32,
            "colorFormatCount": 8,
            "colorFormats": 12,
            "depthReadOnly": 24,
            "depthStencilFormat": 16,
            "label": 4,
            "nextInChain": 0,
            "sampleCount": 20,
            "stencilReadOnly": 28
        },
        "WGPURenderPassColorAttachment": {
            "__size__": 56,
            "clearValue": 24,
            "depthSlice": 8,
            "loadOp": 16,
            "nextInChain": 0,
            "resolveTarget": 12,
            "storeOp": 20,
            "view": 4
        },
        "WGPURenderPassDepthStencilAttachment": {
            "__size__": 36,
            "depthClearValue": 12,
            "depthLoadOp": 4,
            "depthReadOnly": 16,
            "depthStoreOp": 8,
            "stencilClearValue": 28,
            "stencilLoadOp": 20,
            "stencilReadOnly": 32,
            "stencilStoreOp": 24,
            "view": 0
        },
        "WGPURenderPassDescriptor": {
            "__size__": 28,
            "colorAttachmentCount": 8,
            "colorAttachments": 12,
            "depthStencilAttachment": 16,
            "label": 4,
            "nextInChain": 0,
            "occlusionQuerySet": 20,
            "timestampWrites": 24
        },
        "WGPURenderPassDescriptorMaxDrawCount": {
            "__size__": 16,
            "chain": 0,
            "maxDrawCount": 8
        },
        "WGPURenderPassTimestampWrites": {
            "__size__": 12,
            "beginningOfPassWriteIndex": 4,
            "endOfPassWriteIndex": 8,
            "querySet": 0
        },
        "WGPURenderPipelineDescriptor": {
            "__size__": 84,
            "depthStencil": 60,
            "fragment": 80,
            "label": 4,
            "layout": 8,
            "multisample": 64,
            "nextInChain": 0,
            "primitive": 40,
            "vertex": 12
        },
        "WGPURequestAdapterCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPURequestAdapterOptions": {
            "__size__": 24,
            "backendType": 12,
            "compatibilityMode": 20,
            "compatibleSurface": 4,
            "forceFallbackAdapter": 16,
            "nextInChain": 0,
            "powerPreference": 8
        },
        "WGPURequestDeviceCallbackInfo": {
            "__size__": 16,
            "callback": 8,
            "mode": 4,
            "nextInChain": 0,
            "userdata": 12
        },
        "WGPURequiredLimits": {
            "__size__": 152,
            "limits": 8,
            "nextInChain": 0
        },
        "WGPUSamplerBindingLayout": {
            "__size__": 8,
            "nextInChain": 0,
            "type": 4
        },
        "WGPUSamplerDescriptor": {
            "__size__": 48,
            "addressModeU": 8,
            "addressModeV": 12,
            "addressModeW": 16,
            "compare": 40,
            "label": 4,
            "lodMaxClamp": 36,
            "lodMinClamp": 32,
            "magFilter": 20,
            "maxAnisotropy": 44,
            "minFilter": 24,
            "mipmapFilter": 28,
            "nextInChain": 0
        },
        "WGPUShaderModuleDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUShaderModuleSPIRVDescriptor": {
            "__size__": 16,
            "chain": 0,
            "code": 12,
            "codeSize": 8
        },
        "WGPUShaderModuleWGSLDescriptor": {
            "__size__": 12,
            "chain": 0,
            "code": 8
        },
        "WGPUStencilFaceState": {
            "__size__": 16,
            "compare": 0,
            "depthFailOp": 8,
            "failOp": 4,
            "passOp": 12
        },
        "WGPUStorageTextureBindingLayout": {
            "__size__": 16,
            "access": 4,
            "format": 8,
            "nextInChain": 0,
            "viewDimension": 12
        },
        "WGPUSupportedLimits": {
            "__size__": 152,
            "limits": 8,
            "nextInChain": 0
        },
        "WGPUSurfaceCapabilities": {
            "__size__": 28,
            "alphaModeCount": 20,
            "alphaModes": 24,
            "formatCount": 4,
            "formats": 8,
            "nextInChain": 0,
            "presentModeCount": 12,
            "presentModes": 16
        },
        "WGPUSurfaceConfiguration": {
            "__size__": 40,
            "alphaMode": 24,
            "device": 4,
            "format": 8,
            "height": 32,
            "nextInChain": 0,
            "presentMode": 36,
            "usage": 12,
            "viewFormatCount": 16,
            "viewFormats": 20,
            "width": 28
        },
        "WGPUSurfaceDescriptor": {
            "__size__": 8,
            "label": 4,
            "nextInChain": 0
        },
        "WGPUSurfaceDescriptorFromCanvasHTMLSelector": {
            "__size__": 12,
            "chain": 0,
            "selector": 8
        },
        "WGPUSurfaceTexture": {
            "__size__": 12,
            "status": 8,
            "suboptimal": 4,
            "texture": 0
        },
        "WGPUSwapChainDescriptor": {
            "__size__": 28,
            "format": 12,
            "height": 20,
            "label": 4,
            "nextInChain": 0,
            "presentMode": 24,
            "usage": 8,
            "width": 16
        },
        "WGPUTextureBindingLayout": {
            "__size__": 16,
            "multisampled": 12,
            "nextInChain": 0,
            "sampleType": 4,
            "viewDimension": 8
        },
        "WGPUTextureBindingViewDimensionDescriptor": {
            "__size__": 12,
            "chain": 0,
            "textureBindingViewDimension": 8
        },
        "WGPUTextureDataLayout": {
            "__size__": 24,
            "bytesPerRow": 16,
            "nextInChain": 0,
            "offset": 8,
            "rowsPerImage": 20
        },
        "WGPUTextureDescriptor": {
            "__size__": 48,
            "dimension": 12,
            "format": 28,
            "label": 4,
            "mipLevelCount": 32,
            "nextInChain": 0,
            "sampleCount": 36,
            "size": 16,
            "usage": 8,
            "viewFormatCount": 40,
            "viewFormats": 44
        },
        "WGPUTextureViewDescriptor": {
            "__size__": 36,
            "arrayLayerCount": 28,
            "aspect": 32,
            "baseArrayLayer": 24,
            "baseMipLevel": 16,
            "dimension": 12,
            "format": 8,
            "label": 4,
            "mipLevelCount": 20,
            "nextInChain": 0
        },
        "WGPUUncapturedErrorCallbackInfo": {
            "__size__": 12,
            "callback": 4,
            "nextInChain": 0,
            "userdata": 8
        },
        "WGPUVertexAttribute": {
            "__size__": 24,
            "format": 0,
            "offset": 8,
            "shaderLocation": 16
        },
        "WGPUVertexBufferLayout": {
            "__size__": 24,
            "arrayStride": 0,
            "attributeCount": 12,
            "attributes": 16,
            "stepMode": 8
        },
        "WGPUVertexState": {
            "__size__": 28,
            "bufferCount": 20,
            "buffers": 24,
            "constantCount": 12,
            "constants": 16,
            "entryPoint": 8,
            "module": 4,
            "nextInChain": 0
        }
    }
}
);

const structInfo64 = (
{
    "defines": {},
    "structs": {
        "WGPUAdapterProperties": {
            "__size__": 72,
            "adapterType": 56,
            "architecture": 24,
            "backendType": 60,
            "compatibilityMode": 64,
            "deviceID": 32,
            "driverDescription": 48,
            "name": 40,
            "nextInChain": 0,
            "vendorID": 8,
            "vendorName": 16
        },
        "WGPUBindGroupDescriptor": {
            "__size__": 40,
            "entries": 32,
            "entryCount": 24,
            "label": 8,
            "layout": 16,
            "nextInChain": 0
        },
        "WGPUBindGroupEntry": {
            "__size__": 56,
            "binding": 8,
            "buffer": 16,
            "nextInChain": 0,
            "offset": 24,
            "sampler": 40,
            "size": 32,
            "textureView": 48
        },
        "WGPUBindGroupLayoutDescriptor": {
            "__size__": 32,
            "entries": 24,
            "entryCount": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUBindGroupLayoutEntry": {
            "__size__": 104,
            "binding": 8,
            "buffer": 16,
            "nextInChain": 0,
            "sampler": 40,
            "storageTexture": 80,
            "texture": 56,
            "visibility": 12
        },
        "WGPUBlendComponent": {
            "__size__": 12,
            "dstFactor": 8,
            "operation": 0,
            "srcFactor": 4
        },
        "WGPUBlendState": {
            "__size__": 24,
            "alpha": 12,
            "color": 0
        },
        "WGPUBufferBindingLayout": {
            "__size__": 24,
            "hasDynamicOffset": 12,
            "minBindingSize": 16,
            "nextInChain": 0,
            "type": 8
        },
        "WGPUBufferDescriptor": {
            "__size__": 40,
            "label": 8,
            "mappedAtCreation": 32,
            "nextInChain": 0,
            "size": 24,
            "usage": 16
        },
        "WGPUBufferMapCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPUChainedStruct": {
            "__size__": 16,
            "next": 0,
            "sType": 8
        },
        "WGPUColor": {
            "__size__": 32,
            "a": 24,
            "b": 16,
            "g": 8,
            "r": 0
        },
        "WGPUColorTargetState": {
            "__size__": 32,
            "blend": 16,
            "format": 8,
            "nextInChain": 0,
            "writeMask": 24
        },
        "WGPUCommandBufferDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUCommandEncoderDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUCompilationInfo": {
            "__size__": 24,
            "messageCount": 8,
            "messages": 16,
            "nextInChain": 0
        },
        "WGPUCompilationInfoCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPUCompilationMessage": {
            "__size__": 80,
            "length": 48,
            "lineNum": 24,
            "linePos": 32,
            "message": 8,
            "nextInChain": 0,
            "offset": 40,
            "type": 16,
            "utf16Length": 72,
            "utf16LinePos": 56,
            "utf16Offset": 64
        },
        "WGPUComputePassDescriptor": {
            "__size__": 24,
            "label": 8,
            "nextInChain": 0,
            "timestampWrites": 16
        },
        "WGPUComputePassTimestampWrites": {
            "__size__": 16,
            "beginningOfPassWriteIndex": 8,
            "endOfPassWriteIndex": 12,
            "querySet": 0
        },
        "WGPUComputePipelineDescriptor": {
            "__size__": 64,
            "compute": 24,
            "label": 8,
            "layout": 16,
            "nextInChain": 0
        },
        "WGPUConstantEntry": {
            "__size__": 24,
            "key": 8,
            "nextInChain": 0,
            "value": 16
        },
        "WGPUCreateComputePipelineAsyncCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPUCreateRenderPipelineAsyncCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPUDepthStencilState": {
            "__size__": 72,
            "depthBias": 60,
            "depthBiasClamp": 68,
            "depthBiasSlopeScale": 64,
            "depthCompare": 16,
            "depthWriteEnabled": 12,
            "format": 8,
            "nextInChain": 0,
            "stencilBack": 36,
            "stencilFront": 20,
            "stencilReadMask": 52,
            "stencilWriteMask": 56
        },
        "WGPUDeviceDescriptor": {
            "__size__": 112,
            "defaultQueue": 40,
            "deviceLostCallbackInfo": 56,
            "label": 8,
            "nextInChain": 0,
            "requiredFeatureCount": 16,
            "requiredFeatures": 24,
            "requiredLimits": 32,
            "uncapturedErrorCallbackInfo": 88
        },
        "WGPUDeviceLostCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPUExtent3D": {
            "__size__": 12,
            "depthOrArrayLayers": 8,
            "height": 4,
            "width": 0
        },
        "WGPUFragmentState": {
            "__size__": 56,
            "constantCount": 24,
            "constants": 32,
            "entryPoint": 16,
            "module": 8,
            "nextInChain": 0,
            "targetCount": 40,
            "targets": 48
        },
        "WGPUFuture": {
            "__size__": 8,
            "id": 0
        },
        "WGPUFutureWaitInfo": {
            "__size__": 16,
            "completed": 8,
            "future": 0
        },
        "WGPUINTERNAL__USING_DAWN_WEBGPU": {
            "__size__": 0
        },
        "WGPUImageCopyBuffer": {
            "__size__": 32,
            "buffer": 24,
            "layout": 0
        },
        "WGPUImageCopyTexture": {
            "__size__": 32,
            "aspect": 24,
            "mipLevel": 8,
            "origin": 12,
            "texture": 0
        },
        "WGPUInstanceDescriptor": {
            "__size__": 32,
            "features": 8,
            "nextInChain": 0
        },
        "WGPUInstanceFeatures": {
            "__size__": 24,
            "nextInChain": 0,
            "timedWaitAnyEnable": 8,
            "timedWaitAnyMaxCount": 16
        },
        "WGPULimits": {
            "__size__": 144,
            "maxBindGroups": 16,
            "maxBindGroupsPlusVertexBuffers": 20,
            "maxBindingsPerBindGroup": 24,
            "maxBufferSize": 88,
            "maxColorAttachmentBytesPerSample": 116,
            "maxColorAttachments": 112,
            "maxComputeInvocationsPerWorkgroup": 124,
            "maxComputeWorkgroupSizeX": 128,
            "maxComputeWorkgroupSizeY": 132,
            "maxComputeWorkgroupSizeZ": 136,
            "maxComputeWorkgroupStorageSize": 120,
            "maxComputeWorkgroupsPerDimension": 140,
            "maxDynamicStorageBuffersPerPipelineLayout": 32,
            "maxDynamicUniformBuffersPerPipelineLayout": 28,
            "maxInterStageShaderComponents": 104,
            "maxInterStageShaderVariables": 108,
            "maxSampledTexturesPerShaderStage": 36,
            "maxSamplersPerShaderStage": 40,
            "maxStorageBufferBindingSize": 64,
            "maxStorageBuffersPerShaderStage": 44,
            "maxStorageTexturesPerShaderStage": 48,
            "maxTextureArrayLayers": 12,
            "maxTextureDimension1D": 0,
            "maxTextureDimension2D": 4,
            "maxTextureDimension3D": 8,
            "maxUniformBufferBindingSize": 56,
            "maxUniformBuffersPerShaderStage": 52,
            "maxVertexAttributes": 96,
            "maxVertexBufferArrayStride": 100,
            "maxVertexBuffers": 80,
            "minStorageBufferOffsetAlignment": 76,
            "minUniformBufferOffsetAlignment": 72
        },
        "WGPUMultisampleState": {
            "__size__": 24,
            "alphaToCoverageEnabled": 16,
            "count": 8,
            "mask": 12,
            "nextInChain": 0
        },
        "WGPUOrigin3D": {
            "__size__": 12,
            "x": 0,
            "y": 4,
            "z": 8
        },
        "WGPUPipelineLayoutDescriptor": {
            "__size__": 32,
            "bindGroupLayoutCount": 16,
            "bindGroupLayouts": 24,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUPopErrorScopeCallbackInfo": {
            "__size__": 40,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "oldCallback": 24,
            "userdata": 32
        },
        "WGPUPrimitiveDepthClipControl": {
            "__size__": 24,
            "chain": 0,
            "unclippedDepth": 16
        },
        "WGPUPrimitiveState": {
            "__size__": 24,
            "cullMode": 20,
            "frontFace": 16,
            "nextInChain": 0,
            "stripIndexFormat": 12,
            "topology": 8
        },
        "WGPUProgrammableStageDescriptor": {
            "__size__": 40,
            "constantCount": 24,
            "constants": 32,
            "entryPoint": 16,
            "module": 8,
            "nextInChain": 0
        },
        "WGPUQuerySetDescriptor": {
            "__size__": 24,
            "count": 20,
            "label": 8,
            "nextInChain": 0,
            "type": 16
        },
        "WGPUQueueDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUQueueWorkDoneCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPURenderBundleDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPURenderBundleEncoderDescriptor": {
            "__size__": 48,
            "colorFormatCount": 16,
            "colorFormats": 24,
            "depthReadOnly": 40,
            "depthStencilFormat": 32,
            "label": 8,
            "nextInChain": 0,
            "sampleCount": 36,
            "stencilReadOnly": 44
        },
        "WGPURenderPassColorAttachment": {
            "__size__": 72,
            "clearValue": 40,
            "depthSlice": 16,
            "loadOp": 32,
            "nextInChain": 0,
            "resolveTarget": 24,
            "storeOp": 36,
            "view": 8
        },
        "WGPURenderPassDepthStencilAttachment": {
            "__size__": 40,
            "depthClearValue": 16,
            "depthLoadOp": 8,
            "depthReadOnly": 20,
            "depthStoreOp": 12,
            "stencilClearValue": 32,
            "stencilLoadOp": 24,
            "stencilReadOnly": 36,
            "stencilStoreOp": 28,
            "view": 0
        },
        "WGPURenderPassDescriptor": {
            "__size__": 56,
            "colorAttachmentCount": 16,
            "colorAttachments": 24,
            "depthStencilAttachment": 32,
            "label": 8,
            "nextInChain": 0,
            "occlusionQuerySet": 40,
            "timestampWrites": 48
        },
        "WGPURenderPassDescriptorMaxDrawCount": {
            "__size__": 24,
            "chain": 0,
            "maxDrawCount": 16
        },
        "WGPURenderPassTimestampWrites": {
            "__size__": 16,
            "beginningOfPassWriteIndex": 8,
            "endOfPassWriteIndex": 12,
            "querySet": 0
        },
        "WGPURenderPipelineDescriptor": {
            "__size__": 144,
            "depthStencil": 104,
            "fragment": 136,
            "label": 8,
            "layout": 16,
            "multisample": 112,
            "nextInChain": 0,
            "primitive": 80,
            "vertex": 24
        },
        "WGPURequestAdapterCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPURequestAdapterOptions": {
            "__size__": 32,
            "backendType": 20,
            "compatibilityMode": 28,
            "compatibleSurface": 8,
            "forceFallbackAdapter": 24,
            "nextInChain": 0,
            "powerPreference": 16
        },
        "WGPURequestDeviceCallbackInfo": {
            "__size__": 32,
            "callback": 16,
            "mode": 8,
            "nextInChain": 0,
            "userdata": 24
        },
        "WGPURequiredLimits": {
            "__size__": 152,
            "limits": 8,
            "nextInChain": 0
        },
        "WGPUSamplerBindingLayout": {
            "__size__": 16,
            "nextInChain": 0,
            "type": 8
        },
        "WGPUSamplerDescriptor": {
            "__size__": 56,
            "addressModeU": 16,
            "addressModeV": 20,
            "addressModeW": 24,
            "compare": 48,
            "label": 8,
            "lodMaxClamp": 44,
            "lodMinClamp": 40,
            "magFilter": 28,
            "maxAnisotropy": 52,
            "minFilter": 32,
            "mipmapFilter": 36,
            "nextInChain": 0
        },
        "WGPUShaderModuleDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUShaderModuleSPIRVDescriptor": {
            "__size__": 32,
            "chain": 0,
            "code": 24,
            "codeSize": 16
        },
        "WGPUShaderModuleWGSLDescriptor": {
            "__size__": 24,
            "chain": 0,
            "code": 16
        },
        "WGPUStencilFaceState": {
            "__size__": 16,
            "compare": 0,
            "depthFailOp": 8,
            "failOp": 4,
            "passOp": 12
        },
        "WGPUStorageTextureBindingLayout": {
            "__size__": 24,
            "access": 8,
            "format": 12,
            "nextInChain": 0,
            "viewDimension": 16
        },
        "WGPUSupportedLimits": {
            "__size__": 152,
            "limits": 8,
            "nextInChain": 0
        },
        "WGPUSurfaceCapabilities": {
            "__size__": 56,
            "alphaModeCount": 40,
            "alphaModes": 48,
            "formatCount": 8,
            "formats": 16,
            "nextInChain": 0,
            "presentModeCount": 24,
            "presentModes": 32
        },
        "WGPUSurfaceConfiguration": {
            "__size__": 56,
            "alphaMode": 40,
            "device": 8,
            "format": 16,
            "height": 48,
            "nextInChain": 0,
            "presentMode": 52,
            "usage": 20,
            "viewFormatCount": 24,
            "viewFormats": 32,
            "width": 44
        },
        "WGPUSurfaceDescriptor": {
            "__size__": 16,
            "label": 8,
            "nextInChain": 0
        },
        "WGPUSurfaceDescriptorFromCanvasHTMLSelector": {
            "__size__": 24,
            "chain": 0,
            "selector": 16
        },
        "WGPUSurfaceTexture": {
            "__size__": 16,
            "status": 12,
            "suboptimal": 8,
            "texture": 0
        },
        "WGPUSwapChainDescriptor": {
            "__size__": 40,
            "format": 20,
            "height": 28,
            "label": 8,
            "nextInChain": 0,
            "presentMode": 32,
            "usage": 16,
            "width": 24
        },
        "WGPUTextureBindingLayout": {
            "__size__": 24,
            "multisampled": 16,
            "nextInChain": 0,
            "sampleType": 8,
            "viewDimension": 12
        },
        "WGPUTextureBindingViewDimensionDescriptor": {
            "__size__": 24,
            "chain": 0,
            "textureBindingViewDimension": 16
        },
        "WGPUTextureDataLayout": {
            "__size__": 24,
            "bytesPerRow": 16,
            "nextInChain": 0,
            "offset": 8,
            "rowsPerImage": 20
        },
        "WGPUTextureDescriptor": {
            "__size__": 64,
            "dimension": 20,
            "format": 36,
            "label": 8,
            "mipLevelCount": 40,
            "nextInChain": 0,
            "sampleCount": 44,
            "size": 24,
            "usage": 16,
            "viewFormatCount": 48,
            "viewFormats": 56
        },
        "WGPUTextureViewDescriptor": {
            "__size__": 48,
            "arrayLayerCount": 36,
            "aspect": 40,
            "baseArrayLayer": 32,
            "baseMipLevel": 24,
            "dimension": 20,
            "format": 16,
            "label": 8,
            "mipLevelCount": 28,
            "nextInChain": 0
        },
        "WGPUUncapturedErrorCallbackInfo": {
            "__size__": 24,
            "callback": 8,
            "nextInChain": 0,
            "userdata": 16
        },
        "WGPUVertexAttribute": {
            "__size__": 24,
            "format": 0,
            "offset": 8,
            "shaderLocation": 16
        },
        "WGPUVertexBufferLayout": {
            "__size__": 32,
            "arrayStride": 0,
            "attributeCount": 16,
            "attributes": 24,
            "stepMode": 8
        },
        "WGPUVertexState": {
            "__size__": 56,
            "bufferCount": 40,
            "buffers": 48,
            "constantCount": 24,
            "constants": 32,
            "entryPoint": 16,
            "module": 8,
            "nextInChain": 0
        }
    }
}
);

// Make sure we aren't inheriting any of the webgpu.h struct info from
// Emscripten's copy.
for (const k of Object.keys(C_STRUCTS)) {
    if (k.startsWith('WGPU')) {
        delete C_STRUCTS[k];
    }
}

const WGPU_STRUCTS = (MEMORY64 ? structInfo64 : structInfo32).structs;
for (const [k, v] of Object.entries(WGPU_STRUCTS)) {
    C_STRUCTS[k] = v;
}
if (!('WGPUINTERNAL__USING_DAWN_WEBGPU' in C_STRUCTS)) {
    throw new Error('struct_info generation error - need webgpu.h from Dawn, got it from Emscripten');
}
C_STRUCTS.__HAVE_DAWN_WEBGPU_STRUCT_INFO = true;

null;
}}}
