package android.dawn

class VertexState(
    var module: ShaderModule? = null,
    var entryPoint: String? = null,
    var constants: Array<ConstantEntry> = arrayOf(),
    var buffers: Array<VertexBufferLayout> = arrayOf()

) {
}
