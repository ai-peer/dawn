package android.dawn

class CommandEncoder(val handle: Long) {

    external fun beginComputePass(descriptor: ComputePassDescriptor?): ComputePassEncoder
    external fun beginRenderPass(descriptor: RenderPassDescriptor?): RenderPassEncoder
    external fun clearBuffer(buffer: Buffer, offset: Long, size: Long): Unit
    external fun copyBufferToBuffer(
        source: Buffer,
        sourceOffset: Long,
        destination: Buffer,
        destinationOffset: Long,
        size: Long
    ): Unit

    external fun copyBufferToTexture(
        source: ImageCopyBuffer?,
        destination: ImageCopyTexture?,
        copySize: Extent3D?
    ): Unit

    external fun copyTextureToBuffer(
        source: ImageCopyTexture?,
        destination: ImageCopyBuffer?,
        copySize: Extent3D?
    ): Unit

    external fun copyTextureToTexture(
        source: ImageCopyTexture?,
        destination: ImageCopyTexture?,
        copySize: Extent3D?
    ): Unit

    external fun finish(descriptor: CommandBufferDescriptor?): CommandBuffer
    external fun injectValidationError(message: String?): Unit
    external fun insertDebugMarker(markerLabel: String?): Unit
    external fun popDebugGroup(): Unit
    external fun pushDebugGroup(groupLabel: String?): Unit
    external fun resolveQuerySet(
        querySet: QuerySet,
        firstQuery: Int,
        queryCount: Int,
        destination: Buffer,
        destinationOffset: Long
    ): Unit

    external fun setLabel(label: String?): Unit
    external fun writeBuffer(buffer: Buffer, bufferOffset: Long, data: ByteArray?, size: Long): Unit
    external fun writeTimestamp(querySet: QuerySet, queryIndex: Int): Unit
}
