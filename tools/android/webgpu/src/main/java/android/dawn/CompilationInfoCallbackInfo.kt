package android.dawn

class CompilationInfoCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: CompilationInfoCallback? = null,
    var userdata: ByteArray? = null
) {
}
