package android.dawn

class SharedTextureMemory(val handle: Long) {

    external fun beginAccess(
        texture: Texture,
        descriptor: SharedTextureMemoryBeginAccessDescriptor?
    ): Boolean

    external fun createTexture(descriptor: TextureDescriptor?): Texture
    external fun isDeviceLost(): Boolean
    external fun setLabel(label: String?): Unit
}
