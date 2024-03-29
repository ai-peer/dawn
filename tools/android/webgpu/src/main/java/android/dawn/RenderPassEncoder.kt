package android.dawn

class RenderPassEncoder(val handle: Long) {

    external fun beginOcclusionQuery(queryIndex: Int): Unit
    external fun draw(
        vertexCount: Int,
        instanceCount: Int,
        firstVertex: Int,
        firstInstance: Int
    ): Unit

    external fun drawIndexed(
        indexCount: Int,
        instanceCount: Int,
        firstIndex: Int,
        baseVertex: Int,
        firstInstance: Int
    ): Unit

    external fun drawIndexedIndirect(indirectBuffer: Buffer, indirectOffset: Long): Unit
    external fun drawIndirect(indirectBuffer: Buffer, indirectOffset: Long): Unit
    external fun end(): Unit
    external fun endOcclusionQuery(): Unit
    external fun executeBundles(bundleCount: Long, bundles: Array<RenderBundle>): Unit
    external fun insertDebugMarker(markerLabel: String?): Unit
    external fun pixelLocalStorageBarrier(): Unit
    external fun popDebugGroup(): Unit
    external fun pushDebugGroup(groupLabel: String?): Unit
    external fun setBindGroup(
        groupIndex: Int,
        group: BindGroup,
        dynamicOffsetCount: Long,
        dynamicOffsets: IntArray?
    ): Unit

    external fun setBlendConstant(color: Color?): Unit
    external fun setIndexBuffer(buffer: Buffer, format: Int, offset: Long, size: Long): Unit
    external fun setLabel(label: String?): Unit
    external fun setPipeline(pipeline: RenderPipeline): Unit
    external fun setScissorRect(x: Int, y: Int, width: Int, height: Int): Unit
    external fun setStencilReference(reference: Int): Unit
    external fun setVertexBuffer(slot: Int, buffer: Buffer, offset: Long, size: Long): Unit
    external fun setViewport(
        x: Float,
        y: Float,
        width: Float,
        height: Float,
        minDepth: Float,
        maxDepth: Float
    ): Unit

    external fun writeTimestamp(querySet: QuerySet, queryIndex: Int): Unit
}
