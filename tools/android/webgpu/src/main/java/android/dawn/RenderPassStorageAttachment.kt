package android.dawn

class RenderPassStorageAttachment(
    var offset: Long = 0,
    var storage: TextureView? = null,
    var loadOp: Int = LoadOp.Undefined,
    var storeOp: Int = StoreOp.Undefined,
    var clearValue: Color = Color()

) {
}
