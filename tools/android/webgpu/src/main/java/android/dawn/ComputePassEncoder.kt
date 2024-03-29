package android.dawn

class ComputePassEncoder(val handle: Long) {

    external fun dispatchWorkgroups(
        workgroupCountX: Int,
        workgroupCountY: Int,
        workgroupCountZ: Int
    ): Unit

    external fun dispatchWorkgroupsIndirect(indirectBuffer: Buffer, indirectOffset: Long): Unit
    external fun end(): Unit
    external fun insertDebugMarker(markerLabel: String?): Unit
    external fun popDebugGroup(): Unit
    external fun pushDebugGroup(groupLabel: String?): Unit
    external fun setBindGroup(
        groupIndex: Int,
        group: BindGroup,
        dynamicOffsetCount: Long,
        dynamicOffsets: IntArray?
    ): Unit

    external fun setLabel(label: String?): Unit
    external fun setPipeline(pipeline: ComputePipeline): Unit
    external fun writeTimestamp(querySet: QuerySet, queryIndex: Int): Unit
}
