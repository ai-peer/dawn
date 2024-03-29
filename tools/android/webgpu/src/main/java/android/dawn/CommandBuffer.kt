package android.dawn

class CommandBuffer(val handle: Long) {

    external fun setLabel(label: String?): Unit
}
