package android.dawn

class PipelineLayoutPixelLocalStorage(
    var totalPixelLocalStorageSize: Long = 0,
    var storageAttachments: Array<PipelineLayoutStorageAttachment> = arrayOf()

) : PipelineLayoutDescriptor() {
}
