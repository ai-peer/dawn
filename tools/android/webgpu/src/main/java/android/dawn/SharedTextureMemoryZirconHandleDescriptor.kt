package android.dawn

class SharedTextureMemoryZirconHandleDescriptor(
    var memoryFD: Int = 0,
    var allocationSize: Long = 0
) : SharedTextureMemoryDescriptor() {
}
