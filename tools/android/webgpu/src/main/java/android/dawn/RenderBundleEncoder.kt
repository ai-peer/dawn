package android.dawn

class RenderBundleEncoder(val handle: Long) {

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
    external fun finish(descriptor: RenderBundleDescriptor?): RenderBundle
    external fun insertDebugMarker(markerLabel: String?): Unit
    external fun popDebugGroup(): Unit
    external fun pushDebugGroup(groupLabel: String?): Unit
    external fun setBindGroup(
        groupIndex: Int,
        group: BindGroup,
        dynamicOffsetCount: Long,
        dynamicOffsets: IntArray?
    ): Unit

    external fun setIndexBuffer(buffer: Buffer, format: Int, offset: Long, size: Long): Unit
    external fun setLabel(label: String?): Unit
    external fun setPipeline(pipeline: RenderPipeline): Unit
    external fun setVertexBuffer(slot: Int, buffer: Buffer, offset: Long, size: Long): Unit
}
