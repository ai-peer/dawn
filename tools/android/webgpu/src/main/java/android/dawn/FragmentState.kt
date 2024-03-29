package android.dawn

class FragmentState(
    var module: ShaderModule? = null,
    var entryPoint: String? = null,
    var constants: Array<ConstantEntry> = arrayOf(),
    var targets: Array<ColorTargetState> = arrayOf()

) {
}
