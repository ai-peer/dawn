package android.dawn

import org.junit.Test

import org.junit.Assert.assertTrue

class EnumsTest {
    @Test
    fun uniqueEnumsAdapterType() {
        val values = HashSet<AdapterType>()
        arrayOf(
            AdapterType.DiscreteGPU,
            AdapterType.IntegratedGPU,
            AdapterType.CPU,
            AdapterType.Unknown
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsAddressMode() {
        val values = HashSet<AddressMode>()
        arrayOf(
            AddressMode.Undefined,
            AddressMode.ClampToEdge,
            AddressMode.Repeat,
            AddressMode.MirrorRepeat
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsAlphaMode() {
        val values = HashSet<AlphaMode>()
        arrayOf(
            AlphaMode.Opaque,
            AlphaMode.Premultiplied,
            AlphaMode.Unpremultiplied
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBackendType() {
        val values = HashSet<BackendType>()
        arrayOf(
            BackendType.Undefined,
            BackendType.Null,
            BackendType.WebGPU,
            BackendType.D3D11,
            BackendType.D3D12,
            BackendType.Metal,
            BackendType.Vulkan,
            BackendType.OpenGL,
            BackendType.OpenGLES
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBlendFactor() {
        val values = HashSet<BlendFactor>()
        arrayOf(
            BlendFactor.Undefined,
            BlendFactor.Zero,
            BlendFactor.One,
            BlendFactor.Src,
            BlendFactor.OneMinusSrc,
            BlendFactor.SrcAlpha,
            BlendFactor.OneMinusSrcAlpha,
            BlendFactor.Dst,
            BlendFactor.OneMinusDst,
            BlendFactor.DstAlpha,
            BlendFactor.OneMinusDstAlpha,
            BlendFactor.SrcAlphaSaturated,
            BlendFactor.Constant,
            BlendFactor.OneMinusConstant,
            BlendFactor.Src1,
            BlendFactor.OneMinusSrc1,
            BlendFactor.Src1Alpha,
            BlendFactor.OneMinusSrc1Alpha
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBlendOperation() {
        val values = HashSet<BlendOperation>()
        arrayOf(
            BlendOperation.Undefined,
            BlendOperation.Add,
            BlendOperation.Subtract,
            BlendOperation.ReverseSubtract,
            BlendOperation.Min,
            BlendOperation.Max
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBufferBindingType() {
        val values = HashSet<BufferBindingType>()
        arrayOf(
            BufferBindingType.Undefined,
            BufferBindingType.Uniform,
            BufferBindingType.Storage,
            BufferBindingType.ReadOnlyStorage
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBufferMapAsyncStatus() {
        val values = HashSet<BufferMapAsyncStatus>()
        arrayOf(
            BufferMapAsyncStatus.Success,
            BufferMapAsyncStatus.InstanceDropped,
            BufferMapAsyncStatus.ValidationError,
            BufferMapAsyncStatus.Unknown,
            BufferMapAsyncStatus.DeviceLost,
            BufferMapAsyncStatus.DestroyedBeforeCallback,
            BufferMapAsyncStatus.UnmappedBeforeCallback,
            BufferMapAsyncStatus.MappingAlreadyPending,
            BufferMapAsyncStatus.OffsetOutOfRange,
            BufferMapAsyncStatus.SizeOutOfRange
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBufferMapState() {
        val values = HashSet<BufferMapState>()
        arrayOf(
            BufferMapState.Unmapped,
            BufferMapState.Pending,
            BufferMapState.Mapped
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsBufferUsage() {
        val values = HashSet<BufferUsage>()
        arrayOf(
            BufferUsage.None,
            BufferUsage.MapRead,
            BufferUsage.MapWrite,
            BufferUsage.CopySrc,
            BufferUsage.CopyDst,
            BufferUsage.Index,
            BufferUsage.Vertex,
            BufferUsage.Uniform,
            BufferUsage.Storage,
            BufferUsage.Indirect,
            BufferUsage.QueryResolve
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCallbackMode() {
        val values = HashSet<CallbackMode>()
        arrayOf(
            CallbackMode.WaitAnyOnly,
            CallbackMode.AllowProcessEvents,
            CallbackMode.AllowSpontaneous
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsColorWriteMask() {
        val values = HashSet<ColorWriteMask>()
        arrayOf(
            ColorWriteMask.None,
            ColorWriteMask.Red,
            ColorWriteMask.Green,
            ColorWriteMask.Blue,
            ColorWriteMask.Alpha,
            ColorWriteMask.All
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCompareFuntion() {
        val values = HashSet<CompareFunction>()
        arrayOf(
            CompareFunction.Undefined,
            CompareFunction.Never,
            CompareFunction.Less,
            CompareFunction.Equal,
            CompareFunction.LessEqual,
            CompareFunction.Greater,
            CompareFunction.NotEqual,
            CompareFunction.GreaterEqual,
            CompareFunction.Always
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCompilationInfoRequestStatus() {
        val values = HashSet<CompilationInfoRequestStatus>()
        arrayOf(
            CompilationInfoRequestStatus.Success,
            CompilationInfoRequestStatus.InstanceDropped,
            CompilationInfoRequestStatus.Error,
            CompilationInfoRequestStatus.DeviceLost,
            CompilationInfoRequestStatus.Unknown
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCompilationMessageType() {
        val values = HashSet<CompilationMessageType>()
        arrayOf(
            CompilationMessageType.Error,
            CompilationMessageType.Warning,
            CompilationMessageType.Info
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCompositeAlphaMode() {
        val values = HashSet<CompositeAlphaMode>()
        arrayOf(
            CompositeAlphaMode.Auto,
            CompositeAlphaMode.Opaque,
            CompositeAlphaMode.Premultiplied,
            CompositeAlphaMode.Unpremultiplied,
            CompositeAlphaMode.Inherit
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCreatePipelineAsyncStatus() {
        val values = HashSet<CreatePipelineAsyncStatus>()
        arrayOf(
            CreatePipelineAsyncStatus.Success,
            CreatePipelineAsyncStatus.InstanceDropped,
            CreatePipelineAsyncStatus.ValidationError,
            CreatePipelineAsyncStatus.InternalError,
            CreatePipelineAsyncStatus.DeviceLost,
            CreatePipelineAsyncStatus.DeviceDestroyed,
            CreatePipelineAsyncStatus.Unknown
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsCullMode() {
        val values = HashSet<CullMode>()
        arrayOf(
            CullMode.Undefined,
            CullMode.None,
            CullMode.Front,
            CullMode.Back
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsDeviceLostReason() {
        val values = HashSet<DeviceLostReason>()
        arrayOf(
            DeviceLostReason.Undefined,
            DeviceLostReason.Destroyed,
            DeviceLostReason.InstanceDropped,
            DeviceLostReason.FailedCreation
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsErrorFilter() {
        val values = HashSet<ErrorFilter>()
        arrayOf(
            ErrorFilter.Validation,
            ErrorFilter.OutOfMemory,
            ErrorFilter.Internal
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsErrorType() {
        val values = HashSet<ErrorType>()
        arrayOf(
            ErrorType.NoError,
            ErrorType.Validation,
            ErrorType.OutOfMemory,
            ErrorType.Internal,
            ErrorType.Unknown,
            ErrorType.DeviceLost
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsExternalTextureRotation() {
        val values = HashSet<ExternalTextureRotation>()
        arrayOf(
            ExternalTextureRotation.Rotate0Degrees,
            ExternalTextureRotation.Rotate90Degrees,
            ExternalTextureRotation.Rotate180Degrees,
            ExternalTextureRotation.Rotate270Degrees,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsFeatureName() {
        val values = HashSet<FeatureName>()
        arrayOf(
            FeatureName.Undefined,
            FeatureName.DepthClipControl,
            FeatureName.Depth32FloatStencil8,
            FeatureName.TimestampQuery,
            FeatureName.TextureCompressionBC,
            FeatureName.TextureCompressionETC2,
            FeatureName.TextureCompressionASTC,
            FeatureName.IndirectFirstInstance,
            FeatureName.ShaderF16,
            FeatureName.RG11B10UfloatRenderable,
            FeatureName.BGRA8UnormStorage,
            FeatureName.Float32Filterable,
            FeatureName.DawnInternalUsages,
            FeatureName.DawnNative,
            FeatureName.ChromiumExperimentalTimestampQueryInsidePasses,
            FeatureName.ImplicitDeviceSynchronization,
            FeatureName.SurfaceCapabilities,
            FeatureName.TransientAttachments,
            FeatureName.MSAARenderToSingleSampled,
            FeatureName.DualSourceBlending,
            FeatureName.D3D11MultithreadProtected,
            FeatureName.ANGLETextureSharing,
            FeatureName.ChromiumExperimentalSubgroups,
            FeatureName.ChromiumExperimentalSubgroupUniformControlFlow,
            FeatureName.PixelLocalStorageCoherent,
            FeatureName.PixelLocalStorageNonCoherent,
            FeatureName.Unorm16TextureFormats,
            FeatureName.Snorm16TextureFormats,
            FeatureName.MultiPlanarFormatExtendedUsages,
            FeatureName.MultiPlanarFormatP010,
            FeatureName.HostMappedPointer,
            FeatureName.MultiPlanarRenderTargets,
            FeatureName.MultiPlanarFormatNv12a,
            FeatureName.FramebufferFetch,
            FeatureName.BufferMapExtendedUsages,
            FeatureName.AdapterPropertiesMemoryHeaps,
            FeatureName.AdapterPropertiesD3D,
            FeatureName.AdapterPropertiesVk,
            FeatureName.R8UnormStorage,
            FeatureName.FormatCapabilities,
            FeatureName.DrmFormatCapabilities,
            FeatureName.Norm16TextureFormats,
            FeatureName.SharedTextureMemoryVkDedicatedAllocation,
            FeatureName.SharedTextureMemoryAHardwareBuffer,
            FeatureName.SharedTextureMemoryDmaBuf,
            FeatureName.SharedTextureMemoryOpaqueFD,
            FeatureName.SharedTextureMemoryZirconHandle,
            FeatureName.SharedTextureMemoryDXGISharedHandle,
            FeatureName.SharedTextureMemoryD3D11Texture2D,
            FeatureName.SharedTextureMemoryIOSurface,
            FeatureName.SharedTextureMemoryEGLImage,
            FeatureName.SharedFenceVkSemaphoreOpaqueFD,
            FeatureName.SharedFenceVkSemaphoreSyncFD,
            FeatureName.SharedFenceVkSemaphoreZirconHandle,
            FeatureName.SharedFenceDXGISharedHandle,
            FeatureName.SharedFenceMTLSharedEvent,
            FeatureName.SharedBufferMemoryD3D12Resource,
            FeatureName.StaticSamplers,
            FeatureName.YCbCrVulkanSamplers,
            FeatureName.ShaderModuleCompilationOptions,
            FeatureName.DawnLoadResolveTexture
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsFilterMode() {
        val values = HashSet<FilterMode>()
        arrayOf(
            FilterMode.Undefined,
            FilterMode.Nearest,
            FilterMode.Linear
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsFrontFace() {
        val values = HashSet<FrontFace>()
        arrayOf(
            FrontFace.Undefined,
            FrontFace.CCW,
            FrontFace.CW
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsHeapProperty() {
        val values = HashSet<HeapProperty>()
        arrayOf(
            HeapProperty.Undefined,
            HeapProperty.DeviceLocal,
            HeapProperty.HostVisible,
            HeapProperty.HostCoherent,
            HeapProperty.HostUncached,
            HeapProperty.HostCached
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsIndexFormat() {
        val values = HashSet<IndexFormat>()
        arrayOf(
            IndexFormat.Undefined,
            IndexFormat.Uint16,
            IndexFormat.Uint32
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsLoadOp() {
        val values = HashSet<LoadOp>()
        arrayOf(
            LoadOp.Undefined,
            LoadOp.Clear,
            LoadOp.Load,
            LoadOp.ExpandResolveTexture
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsLoggingType() {
        val values = HashSet<LoggingType>()
        arrayOf(
            LoggingType.Verbose,
            LoggingType.Info,
            LoggingType.Warning,
            LoggingType.Error
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsMapMode() {
        val values = HashSet<MapMode>()
        arrayOf(
            MapMode.Read,
            MapMode.Write,
            MapMode.None
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsMipmapFilterMode() {
        val values = HashSet<MipmapFilterMode>()
        arrayOf(
            MipmapFilterMode.Undefined,
            MipmapFilterMode.Nearest,
            MipmapFilterMode.Linear,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsPopErrorScopeStatus() {
        val values = HashSet<PopErrorScopeStatus>()
        arrayOf(
            PopErrorScopeStatus.Success,
            PopErrorScopeStatus.InstanceDropped
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsPowerPreference() {
        val values = HashSet<PowerPreference>()
        arrayOf(
            PowerPreference.Undefined,
            PowerPreference.LowPower,
            PowerPreference.HighPerformance
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsPresentMode() {
        val values = HashSet<PresentMode>()
        arrayOf(
            PresentMode.Fifo,
            PresentMode.Immediate,
            PresentMode.Mailbox
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsPrimitiveTopology() {
        val values = HashSet<PrimitiveTopology>()
        arrayOf(
            PrimitiveTopology.Undefined,
            PrimitiveTopology.PointList,
            PrimitiveTopology.LineList,
            PrimitiveTopology.LineStrip,
            PrimitiveTopology.TriangleList,
            PrimitiveTopology.TriangleStrip
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsQueryType() {
        val values = HashSet<QueryType>()
        arrayOf(
            QueryType.Occlusion,
            QueryType.Timestamp
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsQueueWorkDoneStatus() {
        val values = HashSet<QueueWorkDoneStatus>()
        arrayOf(
            QueueWorkDoneStatus.Success,
            QueueWorkDoneStatus.InstanceDropped,
            QueueWorkDoneStatus.Error,
            QueueWorkDoneStatus.Unknown,
            QueueWorkDoneStatus.DeviceLost
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsRequestAdapterStatus() {
        val values = HashSet<RequestAdapterStatus>()
        arrayOf(
            RequestAdapterStatus.Success,
            RequestAdapterStatus.InstanceDropped,
            RequestAdapterStatus.Unavailable,
            RequestAdapterStatus.Error,
            RequestAdapterStatus.Unknown,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsRequestDeviceStatus() {
        val values = HashSet<RequestDeviceStatus>()
        arrayOf(
            RequestDeviceStatus.Success,
            RequestDeviceStatus.InstanceDropped,
            RequestDeviceStatus.Error,
            RequestDeviceStatus.Unknown,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsSamplerBindingType() {
        val values = HashSet<SamplerBindingType>()
        arrayOf(
            SamplerBindingType.Undefined,
            SamplerBindingType.Filtering,
            SamplerBindingType.NonFiltering,
            SamplerBindingType.Comparison
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsShaderStage() {
        val values = HashSet<ShaderStage>()
        arrayOf(
            ShaderStage.None,
            ShaderStage.Vertex,
            ShaderStage.Fragment,
            ShaderStage.Compute
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsSharedFenceType() {
        val values = HashSet<SharedFenceType>()
        arrayOf(
            SharedFenceType.Undefined,
            SharedFenceType.VkSemaphoreOpaqueFD,
            SharedFenceType.VkSemaphoreSyncFD,
            SharedFenceType.VkSemaphoreZirconHandle,
            SharedFenceType.DXGISharedHandle,
            SharedFenceType.MTLSharedEvent
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsStencilOperation() {
        val values = HashSet<StencilOperation>()
        arrayOf(
            StencilOperation.Undefined,
            StencilOperation.Keep,
            StencilOperation.Zero,
            StencilOperation.Replace,
            StencilOperation.Invert,
            StencilOperation.IncrementClamp,
            StencilOperation.DecrementClamp,
            StencilOperation.IncrementWrap,
            StencilOperation.DecrementWrap
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsStorageTextureAccess() {
        val values = HashSet<StorageTextureAccess>()
        arrayOf(
            StorageTextureAccess.Undefined,
            StorageTextureAccess.WriteOnly,
            StorageTextureAccess.ReadOnly,
            StorageTextureAccess.ReadWrite
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsStoreOp() {
        val values = HashSet<StoreOp>()
        arrayOf(
            StoreOp.Undefined,
            StoreOp.Store,
            StoreOp.Discard
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsSType() {
        val values = HashSet<SType>()
        arrayOf(
            SType.Invalid,
            SType.SurfaceDescriptorFromMetalLayer,
            SType.SurfaceDescriptorFromWindowsHWND,
            SType.SurfaceDescriptorFromXlibWindow,
            SType.SurfaceDescriptorFromCanvasHTMLSelector,
            SType.ShaderModuleSPIRVDescriptor,
            SType.ShaderModuleWGSLDescriptor,
            SType.PrimitiveDepthClipControl,
            SType.SurfaceDescriptorFromWaylandSurface,
            SType.SurfaceDescriptorFromAndroidNativeWindow,
            SType.SurfaceDescriptorFromWindowsCoreWindow,
            SType.ExternalTextureBindingEntry,
            SType.ExternalTextureBindingLayout,
            SType.SurfaceDescriptorFromWindowsSwapChainPanel,
            SType.RenderPassDescriptorMaxDrawCount,
            SType.DepthStencilStateDepthWriteDefinedDawn,
            SType.TextureBindingViewDimensionDescriptor,
            SType.DawnTextureInternalUsageDescriptor,
            SType.DawnEncoderInternalUsageDescriptor,
            SType.DawnInstanceDescriptor,
            SType.DawnCacheDeviceDescriptor,
            SType.DawnAdapterPropertiesPowerPreference,
            SType.DawnBufferDescriptorErrorInfoFromWireClient,
            SType.DawnTogglesDescriptor,
            SType.DawnShaderModuleSPIRVOptionsDescriptor,
            SType.RequestAdapterOptionsLUID,
            SType.RequestAdapterOptionsGetGLProc,
            SType.RequestAdapterOptionsD3D11Device,
            SType.DawnMultisampleStateRenderToSingleSampled,
            SType.DawnRenderPassColorAttachmentRenderToSingleSampled,
            SType.RenderPassPixelLocalStorage,
            SType.PipelineLayoutPixelLocalStorage,
            SType.BufferHostMappedPointer,
            SType.DawnExperimentalSubgroupLimits,
            SType.AdapterPropertiesMemoryHeaps,
            SType.AdapterPropertiesD3D,
            SType.AdapterPropertiesVk,
            SType.DawnComputePipelineFullSubgroups,
            SType.DawnWireWGSLControl,
            SType.DawnWGSLBlocklist,
            SType.DrmFormatCapabilities,
            SType.ShaderModuleCompilationOptions
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsSurfaceGetCurrentTextureStatus() {
        val values = HashSet<SurfaceGetCurrentTextureStatus>()
        arrayOf(
            SurfaceGetCurrentTextureStatus.Success,
            SurfaceGetCurrentTextureStatus.Timeout,
            SurfaceGetCurrentTextureStatus.Outdated,
            SurfaceGetCurrentTextureStatus.Lost,
            SurfaceGetCurrentTextureStatus.OutOfMemory,
            SurfaceGetCurrentTextureStatus.DeviceLost
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureAspect() {
        val values = HashSet<TextureAspect>()
        arrayOf(
            TextureAspect.Undefined,
            TextureAspect.All,
            TextureAspect.StencilOnly,
            TextureAspect.DepthOnly,
            TextureAspect.Plane0Only,
            TextureAspect.Plane1Only,
            TextureAspect.Plane2Only
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureDimension() {
        val values = HashSet<TextureDimension>()
        arrayOf(
            TextureDimension.Undefined,
            TextureDimension._1D,
            TextureDimension._2D,
            TextureDimension._3D
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureFormat() {
        val values = HashSet<TextureFormat>()
        arrayOf(
            TextureFormat.Undefined,
            TextureFormat.R8Unorm,
            TextureFormat.R8Snorm,
            TextureFormat.R8Uint,
            TextureFormat.R8Sint,
            TextureFormat.R16Uint,
            TextureFormat.R16Sint,
            TextureFormat.R16Float,
            TextureFormat.RG8Unorm,
            TextureFormat.RG8Snorm,
            TextureFormat.RG8Uint,
            TextureFormat.RG8Sint,
            TextureFormat.R32Float,
            TextureFormat.R32Uint,
            TextureFormat.R32Sint,
            TextureFormat.RG16Uint,
            TextureFormat.RG16Sint,
            TextureFormat.RG16Float,
            TextureFormat.RGBA8Unorm,
            TextureFormat.RGBA8UnormSrgb,
            TextureFormat.RGBA8Snorm,
            TextureFormat.RGBA8Uint,
            TextureFormat.RGBA8Sint,
            TextureFormat.BGRA8Unorm,
            TextureFormat.BGRA8UnormSrgb,
            TextureFormat.RGB10A2Uint,
            TextureFormat.RGB10A2Unorm,
            TextureFormat.RG11B10Ufloat,
            TextureFormat.RGB9E5Ufloat,
            TextureFormat.RG32Float,
            TextureFormat.RG32Uint,
            TextureFormat.RG32Sint,
            TextureFormat.RGBA16Uint,
            TextureFormat.RGBA16Sint,
            TextureFormat.RGBA16Float,
            TextureFormat.RGBA32Float,
            TextureFormat.RGBA32Uint,
            TextureFormat.RGBA32Sint,
            TextureFormat.Stencil8,
            TextureFormat.Depth16Unorm,
            TextureFormat.Depth24Plus,
            TextureFormat.Depth24PlusStencil8,
            TextureFormat.Depth32Float,
            TextureFormat.Depth32FloatStencil8,
            TextureFormat.BC1RGBAUnorm,
            TextureFormat.BC1RGBAUnormSrgb,
            TextureFormat.BC2RGBAUnorm,
            TextureFormat.BC2RGBAUnormSrgb,
            TextureFormat.BC3RGBAUnorm,
            TextureFormat.BC3RGBAUnormSrgb,
            TextureFormat.BC4RUnorm,
            TextureFormat.BC4RSnorm,
            TextureFormat.BC5RGUnorm,
            TextureFormat.BC5RGSnorm,
            TextureFormat.BC6HRGBUfloat,
            TextureFormat.BC6HRGBFloat,
            TextureFormat.BC7RGBAUnorm,
            TextureFormat.BC7RGBAUnormSrgb,
            TextureFormat.ETC2RGB8Unorm,
            TextureFormat.ETC2RGB8UnormSrgb,
            TextureFormat.ETC2RGB8A1Unorm,
            TextureFormat.ETC2RGB8A1UnormSrgb,
            TextureFormat.ETC2RGBA8Unorm,
            TextureFormat.ETC2RGBA8UnormSrgb,
            TextureFormat.EACR11Unorm,
            TextureFormat.EACR11Snorm,
            TextureFormat.EACRG11Unorm,
            TextureFormat.EACRG11Snorm,
            TextureFormat.ASTC4x4Unorm,
            TextureFormat.ASTC4x4UnormSrgb,
            TextureFormat.ASTC5x4Unorm,
            TextureFormat.ASTC5x4UnormSrgb,
            TextureFormat.ASTC5x5Unorm,
            TextureFormat.ASTC5x5UnormSrgb,
            TextureFormat.ASTC6x5Unorm,
            TextureFormat.ASTC6x5UnormSrgb,
            TextureFormat.ASTC6x6Unorm,
            TextureFormat.ASTC6x6UnormSrgb,
            TextureFormat.ASTC8x5Unorm,
            TextureFormat.ASTC8x5UnormSrgb,
            TextureFormat.ASTC8x6Unorm,
            TextureFormat.ASTC8x6UnormSrgb,
            TextureFormat.ASTC8x8Unorm,
            TextureFormat.ASTC8x8UnormSrgb,
            TextureFormat.ASTC10x5Unorm,
            TextureFormat.ASTC10x5UnormSrgb,
            TextureFormat.ASTC10x6Unorm,
            TextureFormat.ASTC10x6UnormSrgb,
            TextureFormat.ASTC10x8Unorm,
            TextureFormat.ASTC10x8UnormSrgb,
            TextureFormat.ASTC10x10Unorm,
            TextureFormat.ASTC10x10UnormSrgb,
            TextureFormat.ASTC12x10Unorm,
            TextureFormat.ASTC12x10UnormSrgb,
            TextureFormat.ASTC12x12Unorm,
            TextureFormat.ASTC12x12UnormSrgb,
            TextureFormat.R16Unorm,
            TextureFormat.RG16Unorm,
            TextureFormat.RGBA16Unorm,
            TextureFormat.R16Snorm,
            TextureFormat.RG16Snorm,
            TextureFormat.RGBA16Snorm,
            TextureFormat.R8BG8Biplanar420Unorm,
            TextureFormat.R10X6BG10X6Biplanar420Unorm,
            TextureFormat.R8BG8A8Triplanar420Unorm
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureSampleType() {
        val values = HashSet<TextureSampleType>()
        arrayOf(
            TextureSampleType.Undefined,
            TextureSampleType.Float,
            TextureSampleType.UnfilterableFloat,
            TextureSampleType.Depth,
            TextureSampleType.Sint,
            TextureSampleType.Uint,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureUsage() {
        val values = HashSet<TextureUsage>()
        arrayOf(
            TextureUsage.None,
            TextureUsage.CopySrc,
            TextureUsage.CopyDst,
            TextureUsage.TextureBinding,
            TextureUsage.StorageBinding,
            TextureUsage.RenderAttachment,
            TextureUsage.TransientAttachment,
            TextureUsage.StorageAttachment,
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsTextureViewDimension() {
        val values = HashSet<TextureViewDimension>()
        arrayOf(
            TextureViewDimension.Undefined,
            TextureViewDimension._1D,
            TextureViewDimension._2D,
            TextureViewDimension._2DArray,
            TextureViewDimension.Cube,
            TextureViewDimension.CubeArray,
            TextureViewDimension._3D
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsVertexFormat() {
        val values = HashSet<VertexFormat>()
        arrayOf(
            VertexFormat.Undefined,
            VertexFormat.Uint8x2,
            VertexFormat.Uint8x4,
            VertexFormat.Sint8x2,
            VertexFormat.Sint8x4,
            VertexFormat.Unorm8x2,
            VertexFormat.Unorm8x4,
            VertexFormat.Snorm8x2,
            VertexFormat.Snorm8x4,
            VertexFormat.Uint16x2,
            VertexFormat.Uint16x4,
            VertexFormat.Sint16x2,
            VertexFormat.Sint16x4,
            VertexFormat.Unorm16x2,
            VertexFormat.Unorm16x4,
            VertexFormat.Snorm16x2,
            VertexFormat.Snorm16x4,
            VertexFormat.Float16x2,
            VertexFormat.Float16x4,
            VertexFormat.Float32,
            VertexFormat.Float32x2,
            VertexFormat.Float32x3,
            VertexFormat.Float32x4,
            VertexFormat.Uint32,
            VertexFormat.Uint32x2,
            VertexFormat.Uint32x3,
            VertexFormat.Uint32x4,
            VertexFormat.Sint32,
            VertexFormat.Sint32x2,
            VertexFormat.Sint32x3,
            VertexFormat.Sint32x4,
            VertexFormat.Unorm10_10_10_2
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsVertexStepMode() {
        val values = HashSet<VertexStepMode>()
        arrayOf(
            VertexStepMode.Undefined,
            VertexStepMode.VertexBufferNotUsed,
            VertexStepMode.Vertex,
            VertexStepMode.Instance
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsWaitStatus() {
        val values = HashSet<WaitStatus>()
        arrayOf(
            WaitStatus.Success,
            WaitStatus.TimedOut,
            WaitStatus.UnsupportedTimeout,
            WaitStatus.UnsupportedCount,
            WaitStatus.UnsupportedMixedSources,
            WaitStatus.Unknown
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
    @Test
    fun uniqueEnumsWGSLFeatureName() {
        val values = HashSet<WGSLFeatureName>()
        arrayOf(
            WGSLFeatureName.Undefined,
            WGSLFeatureName.ReadonlyAndReadwriteStorageTextures,
            WGSLFeatureName.Packed4x8IntegerDotProduct,
            WGSLFeatureName.UnrestrictedPointerParameters,
            WGSLFeatureName.PointerCompositeAccess,
            WGSLFeatureName.ChromiumTestingUnimplemented,
            WGSLFeatureName.ChromiumTestingUnsafeExperimental,
            WGSLFeatureName.ChromiumTestingExperimental,
            WGSLFeatureName.ChromiumTestingShippedWithKillswitch,
            WGSLFeatureName.ChromiumTestingShipped
        ).forEach { value -> assertTrue("Multiple enums share value $value", values.add(value)) }
    }
}
