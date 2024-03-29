package android.dawn

open class TextureDescriptor(
    var label: String? = null,
    var usage: Int = TextureUsage.None,
    var dimension: Int = TextureDimension._2D,
    var size: Extent3D = Extent3D(),
    var format: Int = TextureFormat.Undefined,
    var mipLevelCount: Int = 1,
    var sampleCount: Int = 1,
    var viewFormats: IntArray = intArrayOf()
) {
}
