package android.dawn

class RenderPassDepthStencilAttachment(
    var view: TextureView? = null,
    var depthLoadOp: Int = LoadOp.Undefined,
    var depthStoreOp: Int = StoreOp.Undefined,
    var depthClearValue: Float = Float.NaN,
    var depthReadOnly: Boolean = false,
    var stencilLoadOp: Int = LoadOp.Undefined,
    var stencilStoreOp: Int = StoreOp.Undefined,
    var stencilClearValue: Int = 0,
    var stencilReadOnly: Boolean = false
) {
}
