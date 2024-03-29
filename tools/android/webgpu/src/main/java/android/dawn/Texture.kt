package android.dawn

class Texture(val handle: Long) {

    external fun createErrorView(descriptor: TextureViewDescriptor?): TextureView
    external fun createView(descriptor: TextureViewDescriptor?): TextureView
    external fun destroy(): Unit
    val depthOrArrayLayers: Int
        external get
    val dimension: Int
        external get
    val format: Int
        external get
    val height: Int
        external get
    val mipLevelCount: Int
        external get
    val sampleCount: Int
        external get
    val usage: Int
        external get
    val width: Int
        external get

    external fun setLabel(label: String?): Unit
}
