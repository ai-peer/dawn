package android.dawn

class RenderPipeline(val handle: Long) {

    external fun getBindGroupLayout(groupIndex: Int): BindGroupLayout
    external fun setLabel(label: String?): Unit
}
