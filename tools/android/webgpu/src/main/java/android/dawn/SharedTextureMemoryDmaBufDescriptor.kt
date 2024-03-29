package android.dawn

class SharedTextureMemoryDmaBufDescriptor(
    var size: Extent3D = Extent3D(),
    var drmFormat: Int = 0,
    var drmModifier: Long = 0,
    var planes: Array<SharedTextureMemoryDmaBufPlane> = arrayOf()

) : SharedTextureMemoryDescriptor() {
}
