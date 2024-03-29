package android.dawn

class RenderPipelineDescriptor(
    var label: String? = null,
    var layout: PipelineLayout? = null,
    var vertex: VertexState = VertexState(),
    var primitive: PrimitiveState = PrimitiveState(),
    var depthStencil: DepthStencilState? = null,
    var multisample: MultisampleState = MultisampleState(),
    var fragment: FragmentState? = null
) {
}
