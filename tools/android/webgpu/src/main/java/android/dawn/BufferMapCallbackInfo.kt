package android.dawn

class BufferMapCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: BufferMapCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
