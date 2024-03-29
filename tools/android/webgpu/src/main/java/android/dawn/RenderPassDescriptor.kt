package android.dawn

open class RenderPassDescriptor(
    var label: String? = null,
    var colorAttachments: Array<RenderPassColorAttachment> = arrayOf(),
    var depthStencilAttachment: RenderPassDepthStencilAttachment? = null,
    var occlusionQuerySet: QuerySet? = null,
    var timestampWrites: RenderPassTimestampWrites? = null
) {
}
