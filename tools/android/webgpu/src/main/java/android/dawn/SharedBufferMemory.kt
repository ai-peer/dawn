package android.dawn

class SharedBufferMemory(val handle: Long) {

    external fun beginAccess(
        buffer: Buffer,
        descriptor: SharedBufferMemoryBeginAccessDescriptor?
    ): Boolean

    external fun createBuffer(descriptor: BufferDescriptor?): Buffer
    external fun isDeviceLost(): Boolean
    external fun setLabel(label: String?): Unit
}
