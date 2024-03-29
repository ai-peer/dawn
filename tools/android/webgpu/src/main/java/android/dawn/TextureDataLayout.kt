package android.dawn

class TextureDataLayout(
    var offset: Long = 0,
    var bytesPerRow: Int = Constants.COPY_STRIDE_UNDEFINED,
    var rowsPerImage: Int = Constants.COPY_STRIDE_UNDEFINED
) {
}
