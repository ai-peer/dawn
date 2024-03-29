package android.dawn

open class SharedTextureMemoryBeginAccessDescriptor(
    var concurrentRead: Boolean = false,
    var initialized: Boolean = false,
    var fences: Array<SharedFence>? = null,
    var signaledValues: LongArray = longArrayOf()
) {
}
