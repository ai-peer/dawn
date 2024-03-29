package android.dawn

class RenderBundleEncoderDescriptor(
    var label: String? = null,
    var colorFormats: IntArray = intArrayOf(),
    var depthStencilFormat: Int = TextureFormat.Undefined,
    var sampleCount: Int = 1,
    var depthReadOnly: Boolean = false,
    var stencilReadOnly: Boolean = false
) {
}
