package android.dawn

class VertexBufferLayout(
    var arrayStride: Long = 0,
    var stepMode: Int = VertexStepMode.Vertex,
    var attributes: Array<VertexAttribute> = arrayOf()

) {
}
