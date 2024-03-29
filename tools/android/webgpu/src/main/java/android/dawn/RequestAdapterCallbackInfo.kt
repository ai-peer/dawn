package android.dawn

class RequestAdapterCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: RequestAdapterCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
