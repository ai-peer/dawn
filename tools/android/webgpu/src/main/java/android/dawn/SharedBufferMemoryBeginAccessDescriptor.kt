package android.dawn

class SharedBufferMemoryBeginAccessDescriptor(
    var initialized: Boolean = false,
    var fences: Array<SharedFence>? = null,
    var signaledValues: LongArray = longArrayOf()
) {
}
