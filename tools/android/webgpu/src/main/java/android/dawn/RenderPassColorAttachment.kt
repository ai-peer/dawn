package android.dawn

open class RenderPassColorAttachment(
    var view: TextureView? = null,
    var depthSlice: Int = Constants.DEPTH_SLICE_UNDEFINED,
    var resolveTarget: TextureView? = null,
    var loadOp: Int = LoadOp.Undefined,
    var storeOp: Int = StoreOp.Undefined,
    var clearValue: Color = Color()

) {
}
