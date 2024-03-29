package android.dawn

class SharedBufferMemoryEndAccessState(
    var initialized: Boolean = false,
    var fences: Array<SharedFence>? = null,
    var signaledValues: LongArray = longArrayOf()
) {
}
