package android.dawn

open class PrimitiveState(
    var topology: Int = PrimitiveTopology.TriangleList,
    var stripIndexFormat: Int = IndexFormat.Undefined,
    var frontFace: Int = FrontFace.CCW,
    var cullMode: Int = CullMode.None

) {
}
