package android.dawn

class SharedTextureMemoryVkImageDescriptor(
    var vkFormat: Int = 0,
    var vkUsageFlags: Int = 0,
    var vkExtent3D: Extent3D = Extent3D()

) : SharedTextureMemoryDescriptor() {
}
