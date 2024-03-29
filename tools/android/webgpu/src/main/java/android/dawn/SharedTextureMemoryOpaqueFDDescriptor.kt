package android.dawn

class SharedTextureMemoryOpaqueFDDescriptor(
    var vkImageCreateInfo: ByteArray = byteArrayOf(),
    var memoryFD: Int = 0,
    var memoryTypeIndex: Int = 0,
    var allocationSize: Long = 0,
    var dedicatedAllocation: Boolean = false
) : SharedTextureMemoryDescriptor() {
}
