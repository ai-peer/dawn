package android.dawn

open class BufferDescriptor(
    var label: String? = null,
    var usage: Int = BufferUsage.None,
    var size: Long = 0,
    var mappedAtCreation: Boolean = false
) {
}
