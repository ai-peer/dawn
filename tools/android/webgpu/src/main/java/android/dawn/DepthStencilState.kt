package android.dawn

open class DepthStencilState(
    var format: Int = TextureFormat.Undefined,
    var depthWriteEnabled: Boolean = false,
    var depthCompare: Int = CompareFunction.Undefined,
    var stencilFront: StencilFaceState = StencilFaceState(),
    var stencilBack: StencilFaceState = StencilFaceState(),
    var stencilReadMask: Int = -0x7FFFFFFF,
    var stencilWriteMask: Int = -0x7FFFFFFF,
    var depthBias: Int = 0,
    var depthBiasSlopeScale: Float = 0.0f,
    var depthBiasClamp: Float = 0.0f
) {
}
