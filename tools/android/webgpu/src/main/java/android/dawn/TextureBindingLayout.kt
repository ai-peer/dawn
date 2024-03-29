package android.dawn

class TextureBindingLayout(
    var sampleType: Int = TextureSampleType.Undefined,
    var viewDimension: Int = TextureViewDimension._2D,
    var multisampled: Boolean = false
) {
}
