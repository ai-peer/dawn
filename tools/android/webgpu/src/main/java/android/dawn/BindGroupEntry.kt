package android.dawn

open class BindGroupEntry(
    var binding: Int = 0,
    var buffer: Buffer? = null,
    var offset: Long = 0,
    var size: Long = Constants.WHOLE_SIZE,
    var sampler: Sampler? = null,
    var textureView: TextureView? = null
) {
}
