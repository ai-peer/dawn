package android.dawn

class CopyTextureForBrowserOptions(
    var flipY: Boolean = false,
    var needsColorSpaceConversion: Boolean = false,
    var srcAlphaMode: Int = AlphaMode.Unpremultiplied,
    var srcTransferFunctionParameters: FloatArray? = null,
    var conversionMatrix: FloatArray? = null,
    var dstTransferFunctionParameters: FloatArray? = null,
    var dstAlphaMode: Int = AlphaMode.Unpremultiplied,
    var internalUsage: Boolean = false
) {
}
