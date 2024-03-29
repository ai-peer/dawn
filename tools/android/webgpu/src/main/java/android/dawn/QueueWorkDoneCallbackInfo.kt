package android.dawn

class QueueWorkDoneCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: QueueWorkDoneCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
