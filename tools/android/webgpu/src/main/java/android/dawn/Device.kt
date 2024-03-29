package android.dawn

class Device(val handle: Long) {

    external fun createBindGroup(descriptor: BindGroupDescriptor?): BindGroup
    external fun createBindGroupLayout(descriptor: BindGroupLayoutDescriptor?): BindGroupLayout
    external fun createBuffer(descriptor: BufferDescriptor?): Buffer
    external fun createCommandEncoder(descriptor: CommandEncoderDescriptor?): CommandEncoder
    external fun createComputePipeline(descriptor: ComputePipelineDescriptor?): ComputePipeline
    external fun createComputePipelineAsync(
        descriptor: ComputePipelineDescriptor?,
        callback: CreateComputePipelineAsyncCallback
    ): Unit

    external fun createErrorBuffer(descriptor: BufferDescriptor?): Buffer
    external fun createErrorExternalTexture(): ExternalTexture
    external fun createErrorShaderModule(
        descriptor: ShaderModuleDescriptor?,
        errorMessage: String?
    ): ShaderModule

    external fun createErrorTexture(descriptor: TextureDescriptor?): Texture
    external fun createExternalTexture(externalTextureDescriptor: ExternalTextureDescriptor?): ExternalTexture
    external fun createPipelineLayout(descriptor: PipelineLayoutDescriptor?): PipelineLayout
    external fun createQuerySet(descriptor: QuerySetDescriptor?): QuerySet
    external fun createRenderBundleEncoder(descriptor: RenderBundleEncoderDescriptor?): RenderBundleEncoder
    external fun createRenderPipeline(descriptor: RenderPipelineDescriptor?): RenderPipeline
    external fun createRenderPipelineAsync(
        descriptor: RenderPipelineDescriptor?,
        callback: CreateRenderPipelineAsyncCallback
    ): Unit

    external fun createSampler(descriptor: SamplerDescriptor?): Sampler
    external fun createShaderModule(descriptor: ShaderModuleDescriptor?): ShaderModule
    external fun createSwapChain(surface: Surface, descriptor: SwapChainDescriptor?): SwapChain
    external fun createTexture(descriptor: TextureDescriptor?): Texture
    external fun destroy(): Unit
    external fun forceLoss(type: Int, message: String?): Unit
    val adapter: Adapter
        external get
    val queue: Queue
        external get

    external fun getSupportedSurfaceUsage(surface: Surface): Int
    external fun hasFeature(feature: Int): Boolean
    external fun importSharedBufferMemory(descriptor: SharedBufferMemoryDescriptor?): SharedBufferMemory
    external fun importSharedFence(descriptor: SharedFenceDescriptor?): SharedFence
    external fun importSharedTextureMemory(descriptor: SharedTextureMemoryDescriptor?): SharedTextureMemory
    external fun injectError(type: Int, message: String?): Unit
    external fun popErrorScope(oldCallback: ErrorCallback): Unit
    external fun pushErrorScope(filter: Int): Unit
    external fun setLabel(label: String?): Unit
    external fun setLoggingCallback(callback: LoggingCallback): Unit
    external fun setUncapturedErrorCallback(callback: ErrorCallback): Unit
    external fun tick(): Unit
    external fun validateTextureDescriptor(descriptor: TextureDescriptor?): Unit
}
