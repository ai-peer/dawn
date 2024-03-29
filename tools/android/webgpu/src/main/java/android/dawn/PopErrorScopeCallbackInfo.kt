package android.dawn

class PopErrorScopeCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: PopErrorScopeCallback? = null,
    var oldCallback: ErrorCallback? = null,
    var userdata: ByteArray? = null
) {
}
