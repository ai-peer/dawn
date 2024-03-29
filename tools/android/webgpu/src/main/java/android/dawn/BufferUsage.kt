package android.dawn

object BufferUsage {
    const val None = 0x00000000;
    const val MapRead = 0x00000001;
    const val MapWrite = 0x00000002;
    const val CopySrc = 0x00000004;
    const val CopyDst = 0x00000008;
    const val Index = 0x00000010;
    const val Vertex = 0x00000020;
    const val Uniform = 0x00000040;
    const val Storage = 0x00000080;
    const val Indirect = 0x00000100;
    const val QueryResolve = 0x00000200;
}
