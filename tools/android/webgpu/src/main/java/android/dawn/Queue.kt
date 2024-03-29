package android.dawn

class Queue(val handle: Long) {

    external fun copyExternalTextureForBrowser(
        source: ImageCopyExternalTexture?,
        destination: ImageCopyTexture?,
        copySize: Extent3D?,
        options: CopyTextureForBrowserOptions?
    ): Unit

    external fun copyTextureForBrowser(
        source: ImageCopyTexture?,
        destination: ImageCopyTexture?,
        copySize: Extent3D?,
        options: CopyTextureForBrowserOptions?
    ): Unit

    external fun onSubmittedWorkDone(callback: QueueWorkDoneCallback): Unit
    external fun setLabel(label: String?): Unit
    external fun submit(commandCount: Long, commands: Array<CommandBuffer>): Unit
    external fun writeBuffer(buffer: Buffer, bufferOffset: Long, data: ByteArray?, size: Long): Unit
    external fun writeTexture(
        destination: ImageCopyTexture?,
        data: ByteArray?,
        dataSize: Long,
        dataLayout: TextureDataLayout?,
        writeSize: Extent3D?
    ): Unit
}
