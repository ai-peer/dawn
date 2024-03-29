package android.dawn

open class BindGroupLayoutEntry(
    var binding: Int = 0,
    var visibility: Int = ShaderStage.None,
    var buffer: BufferBindingLayout = BufferBindingLayout(),
    var sampler: SamplerBindingLayout = SamplerBindingLayout(),
    var texture: TextureBindingLayout = TextureBindingLayout(),
    var storageTexture: StorageTextureBindingLayout = StorageTextureBindingLayout()

) {
}
