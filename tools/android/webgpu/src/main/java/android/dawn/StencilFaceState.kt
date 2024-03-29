package android.dawn

class StencilFaceState(
    var compare: Int = CompareFunction.Always,
    var failOp: Int = StencilOperation.Keep,
    var depthFailOp: Int = StencilOperation.Keep,
    var passOp: Int = StencilOperation.Keep

) {
}
