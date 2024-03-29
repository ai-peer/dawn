package android.dawn

class ImageCopyTexture(
    var texture: Texture? = null,
    var mipLevel: Int = 0,
    var origin: Origin3D = Origin3D(),
    var aspect: Int = TextureAspect.All

) {
}
