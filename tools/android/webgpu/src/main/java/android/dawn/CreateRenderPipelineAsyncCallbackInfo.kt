package android.dawn

class CreateRenderPipelineAsyncCallbackInfo(
    var mode: Int = CallbackMode.WaitAnyOnly,
    var callback: CreateRenderPipelineAsyncCallback? = null,
    var userdata: ByteArray = byteArrayOf()
) {
}
