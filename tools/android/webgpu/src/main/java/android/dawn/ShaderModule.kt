package android.dawn

class ShaderModule(val handle: Long) {

    external fun getCompilationInfo(callback: CompilationInfoCallback): Unit
    external fun setLabel(label: String?): Unit
}
