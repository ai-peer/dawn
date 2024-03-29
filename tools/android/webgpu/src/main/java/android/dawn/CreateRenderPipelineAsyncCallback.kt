package android.dawn

fun interface CreateRenderPipelineAsyncCallback {
    fun callback(
        status: Int,
        pipeline: RenderPipeline?,
        message: String?
    );
}
