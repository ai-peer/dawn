package android.dawn

fun interface CreateComputePipelineAsyncCallback {
    fun callback(
        status: Int,
        pipeline: ComputePipeline?,
        message: String?
    );
}
