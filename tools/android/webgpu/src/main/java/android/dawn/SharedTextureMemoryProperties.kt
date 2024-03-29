package android.dawn

class SharedTextureMemoryProperties(
    var usage: Int = TextureUsage.None,
    var size: Extent3D = Extent3D(),
    var format: Int = TextureFormat.Undefined
) {
}
