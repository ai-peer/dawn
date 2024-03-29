package android.dawn

open class SharedTextureMemoryEndAccessState(
    var initialized: Boolean = false,
    var fences: Array<SharedFence>? = null,
    var signaledValues: LongArray = longArrayOf()
) {
}
