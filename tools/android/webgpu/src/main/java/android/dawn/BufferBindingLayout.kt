package android.dawn

class BufferBindingLayout(
    var type: Int = BufferBindingType.Undefined,
    var hasDynamicOffset: Boolean = false,
    var minBindingSize: Long = 0
) {
}
