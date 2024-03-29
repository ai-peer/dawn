package android.dawn

class TextureViewDescriptor(
    var label: String? = null,
    var format: Int = TextureFormat.Undefined,
    var dimension: Int = TextureViewDimension.Undefined,
    var baseMipLevel: Int = 0,
    var mipLevelCount: Int = Constants.MIP_LEVEL_COUNT_UNDEFINED,
    var baseArrayLayer: Int = 0,
    var arrayLayerCount: Int = Constants.ARRAY_LAYER_COUNT_UNDEFINED,
    var aspect: Int = TextureAspect.All

) {
}
