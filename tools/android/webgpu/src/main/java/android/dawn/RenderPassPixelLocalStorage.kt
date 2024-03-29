package android.dawn

class RenderPassPixelLocalStorage(
    var totalPixelLocalStorageSize: Long = 0,
    var storageAttachments: Array<RenderPassStorageAttachment> = arrayOf()

) : RenderPassDescriptor() {
}
