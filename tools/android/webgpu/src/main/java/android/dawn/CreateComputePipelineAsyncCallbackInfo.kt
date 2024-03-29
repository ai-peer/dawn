package android.dawn

class CreateComputePipelineAsyncCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: CreateComputePipelineAsyncCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
