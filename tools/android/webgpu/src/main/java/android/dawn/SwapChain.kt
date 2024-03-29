package android.dawn

class SwapChain(val handle: Long) {

    val currentTexture: Texture
        external get
    val currentTextureView: TextureView
        external get

    external fun present(): Unit
}
