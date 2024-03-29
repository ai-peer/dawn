package android.dawn

class RequestDeviceCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: RequestDeviceCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
