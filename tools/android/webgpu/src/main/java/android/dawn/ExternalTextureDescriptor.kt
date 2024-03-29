package android.dawn

class ExternalTextureDescriptor(
    var label: String? = null,
    var plane0: TextureView? = null,
    var plane1: TextureView? = null,
    var visibleOrigin: Origin2D = Origin2D(),
    var visibleSize: Extent2D = Extent2D(),
    var doYuvToRgbConversionOnly: Boolean = false,
    var yuvToRgbConversionMatrix: FloatArray? = null,
    var srcTransferFunctionParameters: FloatArray = floatArrayOf(),
    var dstTransferFunctionParameters: FloatArray = floatArrayOf(),
    var gamutConversionMatrix: FloatArray = floatArrayOf(),
    var mirrored: Boolean = false,
    var rotation: Int = ExternalTextureRotation.Rotate0Degrees

) {
}
