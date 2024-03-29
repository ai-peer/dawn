package android.dawn

object BackendType {
    const val Undefined = 0x00000000;
    const val Null = 0x00000001;
    const val WebGPU = 0x00000002;
    const val D3D11 = 0x00000003;
    const val D3D12 = 0x00000004;
    const val Metal = 0x00000005;
    const val Vulkan = 0x00000006;
    const val OpenGL = 0x00000007;
    const val OpenGLES = 0x00000008;
}
