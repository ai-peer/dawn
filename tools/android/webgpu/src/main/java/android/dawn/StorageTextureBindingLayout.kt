package android.dawn

class StorageTextureBindingLayout(
    var access: Int = StorageTextureAccess.Undefined,
    var format: Int = TextureFormat.Undefined,
    var viewDimension: Int = TextureViewDimension._2D

) {
}
