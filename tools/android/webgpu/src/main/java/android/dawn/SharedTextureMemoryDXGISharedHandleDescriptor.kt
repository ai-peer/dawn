package android.dawn

class SharedTextureMemoryDXGISharedHandleDescriptor(
    var handle: ByteArray = byteArrayOf(),
    var useKeyedMutex: Boolean = false
) : SharedTextureMemoryDescriptor() {
}
