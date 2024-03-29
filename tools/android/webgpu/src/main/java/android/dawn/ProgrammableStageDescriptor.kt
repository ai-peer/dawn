package android.dawn

class ProgrammableStageDescriptor(
    var module: ShaderModule? = null,
    var entryPoint: String? = null,
    var constants: Array<ConstantEntry> = arrayOf()

) {
}
