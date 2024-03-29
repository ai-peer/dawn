package android.dawn

class BufferHostMappedPointer(
    var pointer: ByteArray = byteArrayOf(),
    var disposeCallback: Callback? = null,
    var userdata: ByteArray = byteArrayOf()
) : BufferDescriptor() {
}
