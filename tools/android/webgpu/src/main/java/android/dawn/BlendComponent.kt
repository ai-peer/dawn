package android.dawn

class BlendComponent(
    var operation: Int = BlendOperation.Add,
    var srcFactor: Int = BlendFactor.One,
    var dstFactor: Int = BlendFactor.Zero

) {
}
