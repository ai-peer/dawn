package android.dawn

class ColorTargetState(
    var format: Int = TextureFormat.Undefined,
    var blend: BlendState? = null,
    var writeMask: Int = ColorWriteMask.All

) {
}
