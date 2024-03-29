package android.dawn

object TextureUsage {
    const val None = 0x00000000;
    const val CopySrc = 0x00000001;
    const val CopyDst = 0x00000002;
    const val TextureBinding = 0x00000004;
    const val StorageBinding = 0x00000008;
    const val RenderAttachment = 0x00000010;
    const val TransientAttachment = 0x00000020;
    const val StorageAttachment = 0x00000040;
}
